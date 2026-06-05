#include "Fractal.hpp"
#include <filesystem>
#include <fstream>
#include <nfd.h>
#include "../External/clip/clip.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../External/stb/stb_image_write.h"
#include "StructDataEdit.hpp"


Fractal::Fractal()
{
    shaderModel= std::make_unique<ShaderModel>("FRAG_shader.frag");
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
}

void Fractal::drawEditor(bool *open)
{
    if(open&&!(*open))
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if(ImGui::Begin("Shader Editor",open, ImGuiWindowFlags_NoNav))
    {

        switch(shaderModel->status)
        {
        case ShaderModel::COMPILED:
            {
                std::string compilation_time="(f="+std::to_string(shaderModel->get_frag_result().compilation_time)+"s, ";
                compilation_time+="c="+std::to_string(shaderModel->get_compute_result().compilation_time)+"s, ";
                compilation_time+="total="+std::to_string(shaderModel->compilation_time)+"s )";
                ImGui::Text(("Status: COMPILED" +compilation_time).c_str());
            }
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

        if(ImGui::BeginChild("Editor",ImVec2(0,0)),0,ImGuiWindowFlags_NoNav)
        {
            if(shaderEditor->draw())
            {
                sourceChanged();
            }
        }
        ImGui::EndChild();

    }
    ImGui::End();

}


void Fractal::initHLSL()
{
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
    shaderModel->slang_project.setRoot( savePath);
}

bool Fractal::load(const std::string &path)
{
    nlohmann::json j;
    try
    {
        std::ifstream file(path);
        file>>j;
        file.close();
        savePath=path;
        deserialize(j);
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
        if(updatePath&&savePath!=path)
        {
            savePath = path;
            shaderModel->slang_project.setRoot(path);
        }
        saved = true;
        return true;
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
}

void Fractal::setName(std::string newName)
{
    shaderModel->name = std::move(newName);
}

void Fractal::setActive(bool active)
{
    shaderModel->setActive(active);
}

const std::string& Fractal::getError()
{
    return shaderModel->get_error();
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
    // if(p.contains(name)&&p[name].type==typeName)
    // {
    //     p[name].get<T>()=value;
    // }
}

void Fractal::processInput()
{
    ImGuiIO& io = ImGui::GetIO();
    auto& p=shaderModel->uniformParameters;

    p.setParameterIfExists("dt",io.DeltaTime);
    p.setParameterIfExists("extent",glm::vec2((vkbase::extent.width)*shaderModel->getViewArea().z,vkbase::extent.height*shaderModel->getViewArea().w));

    // //mouse data
    if(!io.WantCaptureMouse)
    {
        //mouse pos inside viewport
        float relativePosX=+io.MousePos.x- ImGui::GetWindowViewport()->Pos.x;
        float relativePosY=+io.MousePos.y- ImGui::GetWindowViewport()->Pos.y;



        p.setParameterIfExists("mouse_pos",glm::vec2(relativePosX,relativePosY));
        p.setParameterIfExists("mouse_delta",glm::vec2(io.MouseDelta.x,io.MouseDelta.y));
        p.setParameterIfExists("mouse_wheel_delta",io.MouseWheel);
        p.setParameterIfExists("lmb_down",io.MouseDown[0]?1:0);
        p.setParameterIfExists("rmb_down",io.MouseDown[1]?1:0);
        p.setParameterIfExists("mmb_down",io.MouseDown[2]?1:0);
    }
    else
    {
        p.setParameterIfExists("mouse_wheel_delta",0);
        p.setParameterIfExists("lmb_down",0);
        p.setParameterIfExists("rmb_down",0);
        p.setParameterIfExists("mmb_down",0);
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

            for (auto& [b_id,binding]:shaderModel->uniformParameters.bindings)
            {
                ImGui::PushID(b_id);
                if (ImGui::CollapsingHeader(binding.struct_field.name.c_str()))
                {
                    vkbase::Buffer& buffer=shaderModel->uniformParameters.localBuffers->buffers[b_id];
                    void* data=buffer.map();

                    auto draw_tooltip=[](std::function<void()> draw)
                    {
                        if (ImGui::BeginTooltip())
                        {
                            draw();
                            ImGui::EndTooltip();
                        }

                    };

                    auto width=ImGui::GetContentRegionAvail().x;
                    editStructFieldDynamic(binding.struct_field,data,std::move(draw_tooltip),width);


                    buffer.unmap();
                }
                ImGui::PopID();
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
                    exportCompiling=true;

                    shaderModel->exportOutput( "output", "export_result",
                                               cache.exportType==0?"HLSL":"GLSL", {{0,"P.p.a"},{0,"P.p.t"}}, [this](auto result)
                                               {
                                                   exportCompiling=false;
                                                   if (result.success)
                                                       exportResult.SetText(std::move(result.code));
                                                   else
                                                       exportResult.SetText(std::move(result.debug));
                                               }
                    );
                }

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
                                                         nfdresult_t result = NFD_SaveDialog(&outPath, filterItems, std::size(filterItems), nullptr, (getName() ).c_str());
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
                                                     [this](std::pair<std::string,std::string> fileNameAndType)//sync
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



