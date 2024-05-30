#include "Fractal.hpp"
#include <filesystem>
#include <fstream>
#include <nfd.h>
#include "../External/clip/clip.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../External/stb/stb_image_write.h"

const std::string &Fractal::getMirrorFile()
{
    return fileForMirroring;
}

void Fractal::onUpdateLogic(uint32_t imageIndex)
{
    autoSaveToMirrorFile();
}

Fractal::Fractal()
{
    shaderModel= std::make_unique<ShaderModel>("FRAG_shader.frag");
    //shaderModel->compilers.emplace_back(std::make_unique<SPIRVCompiler>());
    shaderModel->compilers.emplace_back(std::make_unique<HLSLCompiler>());
    shaderModel->setCurrentCompiler("HLSL");

    shaderEditor= new ShaderEditor();
    shaderEditor->setShader(shaderModel.get());


    //export text
    auto palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Background] = ImGui::GetColorU32(ImGuiCol_WindowBg,0.5);
    palette[(int)TextEditor::PaletteIndex::CharLiteral] = ImGui::GetColorU32(ImGuiCol_Text,0.5);
    palette[(int)TextEditor::PaletteIndex::Comment] = 0x9900ff10;
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive]=ImGui::GetColorU32(ImGuiCol_WindowBg,0.7);
    exportResult.SetPalette(palette);
//    if(compiler.languageName=="GLSL")
//        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
//    if(compiler.languageName=="HLSL")
    exportResult.SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
    exportResult.SetShowWhitespaces(false);
    exportResult.SetReadOnly(true);

    setActive(false);
}

void Fractal::ui(bool *showParameters, bool *showEditor, bool* showExport)
{
    parametersUi(showParameters);
    exportWindow(showExport);

    if(shaderEditor)
        drawEditor(showEditor);

    //        if(vkbase::touchData.fingerState[0].down && vkbase::sysRes->getMilliseconds()-vkbase::touchData.fingerState[0].downTime>700)
    //        {
    //            switchMode({vkbase::touchData.fingerState[0].x,vkbase::touchData.fingerState[0].y} );
    //        }
    //        vkbase::sysRes->info("ftime:" +    std::to_string( vkbase::touchData.fingerState[0].time- vkbase::touchData.fingerState[0].downTime));
}

void Fractal::sourceChanged()
{
    shaderModel->setSource(shaderEditor->getSourceCode());
    std::cout<<"Source changed\n";
    savedToMirrorFile = false;
}

void Fractal::drawEditor(bool *open)
{
    if(open&&!(*open))
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if(ImGui::Begin("Shader Editor",open, ImGuiWindowFlags_MenuBar))
    {
        if(fileForMirroring.empty())
        {
            if(ImGui::Button("Bind to file"))
            {
                createMirrorFile();
            }
        } else
        {
            ImGui::Text("%s", ("Bound to file:\t"+fileForMirroring).c_str());
            ImGui::SameLine();
            if(ImGui::Button("Remove file"))
            {
                removeMirrorFile();
            }
        }



        switch(shaderModel->status)
        {
            case ShaderModel::COMPILED:
                ImGui::Text("Status: COMPILED");
                break;
            case ShaderModel::COMPILING:
                ImGui::Text("Status: COMPILING...");
                break;
            case ShaderModel::ERROR:
                ImGui::Text("Status: ERROR");
                newError=true;
                break;
            case ShaderModel::NONE:
                ImGui::Text("Status: NONE");
                break;
        }

        if(ImGui::BeginChild("Editor",ImVec2(0,0),true))
        {
            if(shaderEditor->draw(shaderModel->errorMessage))
            {
                sourceChanged();
            }
        }
        ImGui::EndChild();

    }
    ImGui::End();

}

void Fractal::autoSaveToMirrorFile()
{
    if(mirrorToFileFlag)
    {
        mirrorToFile(fileForMirroring);
        mirrorToFileFlag= false;
    }

    if(!savedToMirrorFile && std::chrono::system_clock::now()-lastSaveTime>std::chrono::milliseconds(500))
    {
        writeMirrorFile();
        lastSaveTime=std::chrono::system_clock::now();
        savedToMirrorFile=true;
        ignoreNextMirrorFileChange=true;
    }
}


void Fractal::mirrorFileChanged(FileAction action)
{
    switch(action)
    {
        case FileAction::Add:
            break;
        case FileAction::Delete:
            writeMirrorFile();
            break;
        case FileAction::Modified:
            if(ignoreNextMirrorFileChange)
                ignoreNextMirrorFileChange=false;
            else
                readFromMirrorFile();
            break;
        case FileAction::Moved:
            writeMirrorFile();
            break;
        default:
            break;
    }
}

void Fractal::initHLSL()
{
    shaderModel->setSource(vkbase::assets["shaders/frag.hlsl"]);
    shaderEditor->setShader(shaderModel.get());
}

const std::string &Fractal::getSavePath() const
{
    return savePath;
}

bool Fractal::isSaved() const
{
    return saved;
}

const std::string &Fractal::getName()
{
    return shaderModel->name;
}

nlohmann::json Fractal::serialize()
{
    if(shaderModel)
    {
        auto j=shaderModel->toJson();
        char* cachePtr=reinterpret_cast<char*>(&cache);
        j["cache"]=std::vector<char>(cachePtr,cachePtr+sizeof(Cache));
        return j;
    }
    return nlohmann::json();
}

void Fractal::deserialize(const nlohmann::json &j)
{
    shaderModel->loadFromJson(j);
    if(j.contains("cache"))
    {
        auto cacheData=j["cache"].get<std::vector<char>>();
        char* cachePtr=reinterpret_cast<char*>(&cache);
        std::copy(cacheData.begin(),cacheData.end(),cachePtr);
    }
    shaderEditor->setShader(shaderModel.get());
}

bool Fractal::load(const std::string &path)
{
    nlohmann::json j;
    try
    {
        std::ifstream file(path);
        file>>j;
        file.close();
        deserialize(j);
        savePath = path;
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }

    return true;
}

bool Fractal::save()
{
    if(!savePath.empty())
    {
        return save(savePath);
    }
    return false;
}

bool Fractal::save(const std::string &path, bool updatePath)
{
    try
    {
        std::ofstream file(path);
        file<<serialize();
        file.close();
        if(updatePath)
            savePath = path;
        saved = true;
        return true;
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
}

void Fractal::createMirrorFile()
{
    std::thread{//open dialog from another thread to avoid blocking main thread
            [this]()
            {
                nfdchar_t *outPath;
                nfdfilteritem_t filterItem[1] = { { "HLSL", "hlsl" } };
                nfdresult_t result = NFD_SaveDialog(&outPath, filterItem, 1, nullptr,(getName()+".hlsl").c_str());
                if (result == NFD_OKAY)
                {
                    fileForMirroring=outPath;
                    mirrorToFileFlag=true;
                    NFD_FreePath(outPath);
                }
                else if (result == NFD_ERROR)
                {
                    std::cerr<< "Error from save dialog: " << NFD_GetError() << std::endl;
                }
            }
    }.detach();
}

void Fractal::mirrorToFile(const std::string &filename, bool initFromFile)
{
    if(addFileToWatch(filename, WRAP_MEMBER_FUNC(mirrorFileChanged)))
    {
        fileForMirroring = filename;
        if(initFromFile && std::filesystem::exists(filename))
        {
            readFromMirrorFile();
        } else
        {
            writeMirrorFile();
        }
    }
}

void Fractal::removeMirrorFile()
{
    removeFileFromWatch(fileForMirroring);
    fileForMirroring="";
}

void Fractal::writeMirrorFile()
{
    if(fileForMirroring.empty())
        return;
    std::ofstream file(fileForMirroring);
    if(!file)
        throw std::runtime_error("Can't open file: "+fileForMirroring);
    file<<shaderModel->getSource();
    file.close();
    ignoreNextMirrorFileChange=true;
}

void Fractal::readFromMirrorFile()
{
    std::cout<<"reading:"+fileForMirroring+"\n";
    if(fileForMirroring.empty())
        return;
    std::ifstream file(fileForMirroring);
    if(!file)
        throw std::runtime_error("Can't open file: "+fileForMirroring);

    //read all file
    std::string str((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    shaderModel->setSource(str);
    shaderEditor->setSourceCode(str);
}

void Fractal::setName(std::string newName)
{
    shaderModel->name = std::move(newName);
}

void Fractal::setActive(bool active)
{
    shaderModel->setActive(active);
    OnLogicUpdateReceiver::enable(active);
}

std::string Fractal::getError()
{
    return shaderModel->errorMessage;
}

bool Fractal::isCompiling()
{
    return shaderModel->status==ShaderModel::COMPILING;
}

bool Fractal::hasCompilationError()
{
    return shaderModel->status==ShaderModel::ERROR;
}

void Fractal::setRenderArea(float x, float y, float w, float h)
{
    shaderModel->setViewArea(glm::vec4 (x/vkbase::extent.width, y/vkbase::extent.height, w/vkbase::extent.width, h/vkbase::extent.height));
}

template<typename T>
void setIfContains(std::map<std::string, UniformParameter>& p,const std::string typeName, const std::string& name, T value)
{
    if(p.contains(name)&&p[name].type==typeName)
    {
        p[name].get<T>()=value;
    }
}

void Fractal::processInput()
{
    ImGuiIO& io = ImGui::GetIO();
    auto& p=shaderModel->uniformParameters.activeParameters;

    //
    if(p.contains("extent")&&p["extent"].type=="vec2")
        p["extent"].get<glm::vec2>()=glm::vec2((vkbase::extent.width)*shaderModel->getViewArea().z,vkbase::extent.height*shaderModel->getViewArea().w);
    if(p.contains("t")&&p["t"].type=="float")
        p["t"].get<float>()+=io.DeltaTime;
    if(p.contains("dt")&&p["dt"].type=="float")
        p["dt"].get<float>()=io.DeltaTime;

    //mouse data
    if(!io.WantCaptureMouse)
    {
        if(p.contains("mouse_pos")&&p["mouse_pos"].type=="vec2")
            p["mouse_pos"].get<glm::vec2>()=glm::vec2(io.MousePos.x,io.MousePos.y);
        if(p.contains("mouse_delta")&&p["mouse_delta"].type=="vec2")
            p["mouse_delta"].get<glm::vec2>()=glm::vec2(io.MouseDelta.x,io.MouseDelta.y);
        if(p.contains("mouse_wheel_delta")&&p["mouse_wheel_delta"].type=="float")
            p["mouse_wheel_delta"].get<float>()=io.MouseWheel;

        if(p.contains("mouse_pos")&&p["mouse_pos"].type=="dvec2")
            p["mouse_pos"].get<glm::dvec2>()=glm::dvec2(io.MousePos.x,io.MousePos.y);
        if(p.contains("mouse_delta")&&p["mouse_delta"].type=="dvec2")
            p["mouse_delta"].get<glm::dvec2>()=glm::dvec2(io.MouseDelta.x,io.MouseDelta.y);
        if(p.contains("mouse_wheel_delta")&&p["mouse_wheel_delta"].type=="double")
            p["mouse_wheel_delta"].get<double>()=io.MouseWheel;

        if(p.contains("lmb_down")&&p["lmb_down"].type=="uint")
            p["lmb_down"].get<uint32_t>()=io.MouseDown[0];
        if(p.contains("rmb_down")&&p["rmb_down"].type=="uint")
            p["rmb_down"].get<uint32_t>()=io.MouseDown[1];
        if(p.contains("mmb_down")&&p["mmb_down"].type=="uint")
            p["mmb_down"].get<uint32_t>()=io.MouseDown[2];
    }
    else
    {
        setIfContains(p, "mouse_delta","vec2", glm::vec2(0));
        setIfContains(p, "mouse_wheel_delta","float", 0);

        setIfContains(p, "mouse_delta","dvec2", glm::dvec2(0));
        setIfContains(p, "mouse_wheel_delta","double", 0);
    }


    float* keys=shaderModel->keys;
    //keyboard data
    if(!io.WantCaptureKeyboard)
    {
        for(int i=0;i<ImGuiKey_NamedKey_COUNT;i++)
        {
            if(ImGui::IsKeyDown(static_cast<ImGuiKey>(i+ImGuiKey_NamedKey_BEGIN-1)))
            {
                if(keys[i]<=0)
                    keys[i]=2;
                else
                    keys[i]=1;
            }
            else
            {
                if(keys[i]>=1)
                    keys[i]=-1;
                else
                    keys[i]=0;
            }
        }
    }
    else
    {
        for(int i=0;i<ImGuiKey_NamedKey_COUNT;i++)
        {
            if(keys[i]>0)
                keys[i]=-1;
            else
                keys[i]=0;
        }
    }
}

void Fractal::parametersUi(bool *showParameters)
{
    ImGuiIO& io = ImGui::GetIO();

    if(!showParameters||*showParameters)
    {
        if(ImGui::Begin("Fractal parameters", showParameters))
        {
            float floatSpeed=0.01;
            float intSpeed=0.1;
            std::map<int, UniformParameter*> sortedParameters;
            for(auto&[n,p]: shaderModel->uniformParameters.activeParameters)
            {
                sortedParameters[p.offset]=&p;
            }

            for(auto&[off,p]: sortedParameters)
            {
                const char* floatFormat="%.5f";
                const std::string& name=p->name;
                if(p->type=="int")
                    ImGui::DragInt(name.c_str(), reinterpret_cast<int*>(p->data.data()), intSpeed);
                else if(p->type=="float")
                    ImGui::DragFloat(name.c_str(), reinterpret_cast<float *>(p->data.data()), floatSpeed, 0, 0, floatFormat);
                else if(p->type=="double")
                    ImGui::DragScalar(name.c_str(), ImGuiDataType_Double, &p->get<double>(), floatSpeed, 0, 0, floatFormat);
                else if(p->type=="uint")
                    ImGui::DragScalar(name.c_str(), ImGuiDataType_U32, &p->get<uint32_t>(), intSpeed);
                else if(p->type=="vec2")
                    ImGui::DragFloat2(name.c_str(), reinterpret_cast<float *>(p->data.data()), floatSpeed, 0, 0, floatFormat);
                else if(p->type=="vec3")
                {
                    ImGui::DragFloat3(name.c_str(), reinterpret_cast<float *>(p->data.data()), floatSpeed, 0, 0, floatFormat);
                    ImGui::ColorEdit3(("###"+name+" color").c_str(), reinterpret_cast<float *>(p->data.data()));
                }
                else if(p->type=="vec4")
                {
                    ImGui::DragFloat4(name.c_str(), reinterpret_cast<float *>(p->data.data()), floatSpeed, 0, 0, floatFormat);
                    ImGui::ColorEdit4(("###"+name+" color").c_str(), reinterpret_cast<float *>(p->data.data()));
                }
                else if(p->type=="dvec2")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_Double, p->data.data(), 2, floatSpeed, 0, 0, floatFormat);
                else if(p->type=="dvec3")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_Double, p->data.data(), 3, floatSpeed, 0, 0, floatFormat);
                else if(p->type=="dvec4")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_Double, p->data.data(), 4, floatSpeed, 0, 0, floatFormat);
                else if(p->type=="ivec2")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_S32, p->data.data(), 2, intSpeed);
                else if(p->type=="ivec3")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_S32, p->data.data(), 3, intSpeed);
                else if(p->type=="ivec4")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_S32, p->data.data(), 4, intSpeed);
                else if(p->type=="uvec2")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_U32, p->data.data(), 2, intSpeed);
                else if(p->type=="uvec3")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_U32, p->data.data(), 3, intSpeed);
                else if(p->type=="uvec4")
                    ImGui::DragScalarN(name.c_str(), ImGuiDataType_U32, p->data.data(), 4, intSpeed);
                else
                    ImGui::Text("Parameter: %s has unsupported type %s (off=%d, size=%zu)", name.c_str(), p->type.c_str(), p->offset, p->data.size());
            }
        }
        ImGui::End();
    }
}

void Fractal::exportWindow(bool *show)
{
    if(!show||*show)
    {
        if(ImGui::Begin("Export", show))
        {

            ImGui::RadioButton("HLSL", &cache.exportType, 0);
            ImGui::RadioButton("GLSL", &cache.exportType, 1);
            ImGui::RadioButton("Bitmap", &cache.exportType, 2);

            if(shaderModel->status != ShaderModel::COMPILED)
            {
                ImGui::Text("Can't export, while comilling or if there are errors");
            }
            else if(cache.exportType<2)
            {
                ImGui::Checkbox("Only body", &cache.exportOnlyBody);
               if(exportCompiling)
                {
                    ImGui::Text("Compiling...");
                } else if(ImGui::Button("Export"))
                {
                    exportCompiling = true;
                    exportParallelTaskManager.runTask([this]() -> std::string
                                                      {
                                                          try
                                                          {
                                                                if(cache.exportType==0)
                                                                    return shaderModel->exportOutput("generated", "HLSL", cache.exportOnlyBody);
                                                                else
                                                                    return shaderModel->exportOutput("generated", "GLSL", cache.exportOnlyBody);
                                                          }
                                                          catch(const std::exception &e)
                                                          {
                                                              return e.what();
                                                          }
                                                      },
                                                      [this](std::string source)
                                                      {
                                                          exportResult.SetText(source);
                                                          exportCompiling = false;
                                                      });
                }


                exportParallelTaskManager.finish();

                if(ImGui::Button("Copy to clipboard"))
                {
                    ImGui::SetClipboardText(exportResult.GetText().c_str());
                }

                exportResult.Render("Export", ImVec2(0, 0), true);
            }
            else
            {
                ImGui::InputInt("Width", &cache.bmpWidth);
                ImGui::InputInt("Height", &cache.bmpHeight);
                ImGui::InputText("Call", cache.callBmpExport, sizeof(cache.callBmpExport));
                ImGui::SameLine();
                //question mark
                ImGui::Button("?", ImVec2(20, 20));
                if(ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("HLSL expression to call generated function for each pixel."
                                "\nuv parameter is relative coordinates of pixel in range [0,1](0,0 is bottom left corner)."
                                "\nYou can export to HLSL to see function signature");
                    ImGui::EndTooltip();
                }

                if(ImGui::Button("Generate"))
                {
                    shaderModel->generateBitmap(cache.bmpWidth, cache.bmpHeight, "generated", cache.callBmpExport,vkbase::imgui::getIMGUIDescriptorSetLayout());
                }
                ImGui::SameLine();
                ImGui::Text("Render time(%dx%d): %f ms",(int)shaderModel->getBitmapGenerator().getWidth(),
                            (int)shaderModel->getBitmapGenerator().getHeight(),
                            shaderModel->getBitmapGenerator().getRenderTime()*1000);
                if(shaderModel->getBitmapDescriptorSet())
                {
                    glm::vec2 size=shaderModel->getBitmapSize();
                    if(ImGui::Button("Copy to clipboard"))
                    {
                        auto bmp=shaderModel->getBitmap();

                        clip::image_spec spec;
                        spec.width = size.x;
                        spec.height = size.y;
                        spec.bits_per_pixel = 32;
                        spec.bytes_per_row = spec.width*4;
                        spec.red_mask = 0xff;
                        spec.green_mask = 0xff00;
                        spec.blue_mask = 0xff0000;
                        spec.alpha_mask = 0xff000000;
                        spec.red_shift = 0;
                        spec.green_shift = 8;
                        spec.blue_shift = 16;
                        spec.alpha_shift = 24;
                        clip::image img(bmp.data(), spec);
                        clip::set_image(img);
                    }

                    if(ImGui::Button("Save"))
                    {
                        imageSaveTaskManager.runTask([this]() -> std::pair<std::string,std::string>
                                                     {

                                                         nfdchar_t *outPath;
                                                         nfdfilteritem_t filterItems[] = {{"PNG", "png" }, {"BMP", "bmp" }, {"JPEG", "jpg" }, {"TGA", "tga" }};
                                                         nfdresult_t result = NFD_SaveDialog(&outPath, filterItems, std::size(filterItems), nullptr, (getName() + ".tga").c_str());
                                                         if (result == NFD_OKAY)
                                                         {
                                                             std::string filename = outPath;
                                                             std::string extension = filename.substr(filename.find_last_of(".") + 1);

                                                             NFD_FreePath(outPath);
                                                             return std::pair<std::string,std::string>{filename,extension};
                                                         }
                                                         else if (result == NFD_CANCEL)
                                                         {
                                                             //std::cout<< "User pressed cancel." << std::endl;
                                                         }
                                                         else if (result == NFD_ERROR)
                                                         {
                                                             std::cerr<< "Error from save dialog: " << NFD_GetError() << std::endl;
                                                         }

                                                         return std::pair<std::string,std::string>{"",""};
                                                     },
                                                     [this](std::pair<std::string,std::string> fileNameAndType)
                                                     {
                                                        std::string extension=fileNameAndType.second;
                                                        std::string fileName=fileNameAndType.first;

                                                        if(!fileName.empty())
                                                        {
                                                            auto bmp=shaderModel->getBitmap();
                                                            int width=shaderModel->getBitmapGenerator().getWidth();
                                                            int height=shaderModel->getBitmapGenerator().getHeight();
                                                            if(extension=="png")
                                                            {
                                                                stbi_write_png(fileName.c_str(), width, height, 4, bmp.data(), width*4);
                                                            }
                                                            else if(extension=="bmp")
                                                            {
                                                                stbi_write_bmp(fileName.c_str(), width, height, 4, bmp.data());
                                                            }
                                                            else if(extension=="jpg")
                                                            {
                                                                stbi_write_jpg(fileName.c_str(), width, height, 4, bmp.data(), 100);
                                                            }
                                                            else if(extension=="tga")
                                                            {
                                                                stbi_write_tga(fileName.c_str(), width, height, 4, bmp.data());
                                                            }
                                                        }
                                                     });
                    }
                    ImGui::Image((ImTextureID) shaderModel->getBitmapDescriptorSet(), ImVec2(size.x, size.y));
                }
            }
        }
        ImGui::End();
    }
}



