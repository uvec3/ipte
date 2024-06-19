#include "SDL.h"
#include <nfd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include "Fractal/Fractal.hpp"
#include <filesystem>

struct Show
{
    bool any= true;
    bool log=true;
    bool errorLog=true;
    bool editor=true;
    bool parameters=true;
    bool debugInfo=true;
    bool exportWindow=false;
    bool fullscreen=false;
} show;

std::vector<Fractal*> fractals;
int activeFractalIndex = -1;
bool exitFlag = false;
int switchToTab = -1;//switch to this tab on next ui update
TextEditor errorLog;
double lastSaveTime = 0;
const double autoSaveInterval = 3.0;
nlohmann::json cacheData;
std::deque<Fractal*> lastTabs;
bool ctrlTabMode = false;
int lastTabOffset = 0;

std::map<std::string,std::map<std::string ,std::string>> templates;

void initTemplates()
{
    for(auto& [path, data]:vkbase::assets)
    {
        std::string prefix = "templates/";
        if(path.starts_with(prefix))
        {
            auto pos = path.find('/',prefix.size());
            std::string category = path.substr(prefix.size(),pos-prefix.size());
            std::string name = path.substr(pos+1);
            if(name.ends_with(".fract"))
                name = name.substr(0,name.size()-6);
            templates[category][name]=data;
        }
    }
}

int getIndexOfFractal(Fractal* fractal)
{
    for(int i = 0; i < fractals.size(); ++i)
    {
        if(fractals[i] == fractal)
        {
            return i;
        }
    }
    return -1;
}

void switchActiveFractal(int index)//set active fractal and deactivate others
{
    if(!ctrlTabMode)
    {
        //remove from last tabs
        lastTabs.erase(std::remove(lastTabs.begin(), lastTabs.end(), fractals[index]), lastTabs.end());
        lastTabs.push_front(fractals[index]);
    }

    for(int i = 0; i < fractals.size(); ++i)
    {
        if(i == index)
        {
            fractals[i]->setActive(true);
        }
        else
        {
            fractals[i]->setActive(false);
        }
    }
    activeFractalIndex = index;
}



void addRecent(const std::string &name, const std::string &path)
{
    if(!cacheData["recent"].is_null())
        cacheData["recent"].erase(std::remove_if(cacheData["recent"].begin(),cacheData["recent"].end(),[&](const nlohmann::json& j){return j["path"]==path;}),cacheData["recent"].end());
    cacheData["recent"].push_back({{"name",name},{"path",path}});
}

void saveAs(bool copy=false)
{
    std::thread{//open save file dialog from another thread to avoid blocking main thread
        [copy]()
        {
            nfdchar_t *outPath;
            nfdfilteritem_t filterItem[1] = { { "Fractal", "fract" } };
            nfdresult_t result = NFD_SaveDialog(&outPath, filterItem, 1, nullptr,fractals[activeFractalIndex]->getName().c_str());
            if (result == NFD_OKAY)
            {
                std::string filename = std::filesystem::path(outPath).filename().string();
                filename = filename.substr(0, filename.find_last_of('.'));

                std::string currentName = fractals[activeFractalIndex]->getName();
                fractals[activeFractalIndex]->setName(filename);
                fractals[activeFractalIndex]->save(outPath, !copy);
                if(copy)
                {
                    fractals[activeFractalIndex]->setName(currentName);
                }
                addRecent(filename,outPath);
                NFD_FreePath(outPath);
            }
            else if (result == NFD_CANCEL)
            {
                //std::cout<< "User pressed cancel." << std::endl;w
            }
            else if (result == NFD_ERROR)
            {
                std::cerr<< "Error from save dialog: " << NFD_GetError() << std::endl;
            }
        }
    }.detach();
}

std::atomic <bool> fractalToOpen = false;
std::string pathToOpen;


//open file dialog from another thread and save path to open in main thread
void open()
{
    if(fractalToOpen)//previous fractal is not loaded yet
        return;

    std::thread{//open file open dialog from another thread to avoid blocking main thread
        []()
        {
            nfdchar_t *outPath;
            nfdfilteritem_t filterItem[1] = {{"Fractal", "fract"}};
            nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 1, nullptr);
            if(result == NFD_OKAY)
            {
                pathToOpen = outPath;
                fractalToOpen = true;
                NFD_FreePath(outPath);
            } else if(result == NFD_CANCEL)
            {
                std::cout << "User pressed cancel." << std::endl;
            } else if(result == NFD_ERROR)
            {
                std::cerr << "Error from open dialog: " << NFD_GetError() << std::endl;
            }
        }
    }.detach();
}

void saveAll()
{
    nlohmann::json j;
    for(int i = 0; i < fractals.size(); ++i)
        {
            std::string path=fractals[i]->getSavePath();
            if(path.empty())
            {
                j["unsaved"][fractals[i]->getName()]=fractals[i]->serialize();
                j["opened"][i]["unsaved"]=fractals[i]->getName();
            }
            else
            {
                fractals[i]->save();
                j["opened"][i]["path"]=path;
            }

            j["opened"][i]["mirror"]=fractals[i]->getMirrorFile();
            if(i==activeFractalIndex)
            {
                j["opened"][i]["active"]=true;
            }
            else
            {
                j["opened"][i]["active"]=false;
            }
        }
    j["cache"]=cacheData;
    j["font_size"]=vkbase::imgui::getFontSize();
    j["show"]=std::vector<char>(reinterpret_cast<char*>(&show),reinterpret_cast<char*>(&show)+sizeof(show));
    j["imgui"]=  ImGui::SaveIniSettingsToMemory();

    std::ofstream file("cache.json");
    file<<j.dump(4);



    file.close();
}

void autoSave()
{
    auto now = vkbase::sys::getMilliseconds();
    if(now-lastSaveTime>autoSaveInterval*1000)
    {
        lastSaveTime = now;
        saveAll();
    }
}

//load fractal from file
bool loadFractalFromFile(const std::string &path)
{
    auto* fractal = new Fractal();
    if(fractal->load(path))
    {
        fractals.push_back(fractal);
        switchToTab =static_cast<int>(fractals.size()-1);
        addRecent(fractals.back()->getName(),path);
        return true;
    }
    else
    {
        delete fractal;
        return false;
    }
}

bool loadFractal(const nlohmann::json &j)
{
    auto* fractal = new Fractal();
    try
    {
        fractal->deserialize(j);
        fractals.push_back(fractal);
        switchToTab =static_cast<int>(fractals.size()-1);
    }
    catch(const std::exception &e)
    {
        std::cout << "Error while loading fractal: " + std::string(e.what());
        delete fractal;
        return false;
    }
    return true;
}

void loadAll()
{
    int activeTabIndex = -1;
    try
    {
        std::ifstream file("cache.json");
        nlohmann::json j;
        if(file)
            file >> j;
        else
            j=nlohmann::json::parse(vkbase::assets["other/cache.json"]);
        file.close();

        for(auto& f:j["opened"])
        {
            if(f.contains("path"))
            {
                if(!loadFractalFromFile(f["path"]))
                    continue;
            }
            else if(f.contains("unsaved"))
            {
                if(!loadFractal(j["unsaved"][f["unsaved"]]))
                    continue;
            }
            if(f.contains("mirror"))
            {
                if(!f["mirror"].get<std::string >().empty())
                    fractals.back()->mirrorToFile(f["mirror"]);
            }
            if(f.contains("active")&&f["active"])
            {
                activeTabIndex = static_cast<int>(fractals.size()-1);
            }
        }
        lastTabs.clear();
        for(auto& fractal:fractals)
        {
            lastTabs.push_back(fractal);
        }
        if(activeTabIndex!=-1)
        {
            switchToTab = activeTabIndex;
        }
        if(j.contains("show"))
            show = *reinterpret_cast<Show*>(j["show"].get<std::vector<char>>().data());
        if(j.contains("font_size"))
            vkbase::imgui::setFontSize(j["font_size"].get<float>());
        if(j.contains("imgui"))
        {
            auto data = j["imgui"].get<std::string>();
            ImGui::LoadIniSettingsFromMemory(data.c_str() , data.size());
        }
        cacheData=j["cache"];
    }
    catch(const std::exception &e)
    {
        std::cout << "Error while loading cache data: " + std::string(e.what());
    }
}

void newFractal(const std::string& name, const nlohmann::json& data)
{
    std::string alteredName = name;
    bool nameAlreadyTaken = true;
    for(int i=1;nameAlreadyTaken;++i)
    {
        nameAlreadyTaken = false;
        for(auto& fractal: fractals)
        {
            if(fractal->getName() == alteredName)
            {
                nameAlreadyTaken = true;
                break;
            }
        }
        if(nameAlreadyTaken)
        {
            alteredName= name +"("+std::to_string(i)+")";
        }
    }

    loadFractal(data);
    fractals.back()->setName(alteredName);
}

void closeFractal(int fractalToClose)
{
    lastTabs.erase(std::remove(lastTabs.begin(), lastTabs.end(), fractals[fractalToClose]), lastTabs.end());
    delete fractals[fractalToClose];
    fractals.erase(fractals.begin()+fractalToClose);
    if(activeFractalIndex>=fractals.size())
        activeFractalIndex=static_cast<int>(fractals.size()-1) ;
}



void drawMenuBarUI()
{
    static bool showMenu = true;

    //show menu if mouse is over it for fullscreen mode when ui is hidden
    if(!show.fullscreen || show.any || ImGui::GetIO().MousePos.y<ImGui::GetMainViewport()->Pos.y+vkbase::imgui::getFontSize()*2||
    ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftCtrl))||ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_RightCtrl)))
    {
        showMenu = true;
    }


   if(!showMenu)//hide menu
       return;


    if(ImGui::BeginMainMenuBar())
    {

        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::BeginMenu("New", "Ctrl+N"))
            {

                for(auto& [category, t]:templates)
                {
                    if(ImGui::BeginMenu(category.c_str()))
                    {
                        for(auto& [name, data]:t)
                        {
                            if(ImGui::MenuItem(name.c_str()))
                            {
                                newFractal(name,nlohmann::json::parse(data));
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndMenu();
            }
            if(ImGui::MenuItem("Open", "Ctrl+O"))
            {
                open();
            }
            if(ImGui::MenuItem("Save", "Ctrl+S"))
            {
                if(!fractals[activeFractalIndex]->save())
                {
                    saveAs();
                }
            }
            if(ImGui::MenuItem("Save As"))
            {
                saveAs();
            }
            if(ImGui::MenuItem("Save Copy As", "Ctrl+Shift+S"))
            {
                saveAs(true);
            }
            if(ImGui::MenuItem("Save All", "Ctrl+Shift+A"))
            {
                saveAll();
            }
            if(ImGui::MenuItem("Close", "Ctrl+W"))
            {
                closeFractal(activeFractalIndex);
            }
            if(ImGui::BeginMenu("Recent"))
            {
                for(auto& recent:std::views::reverse(cacheData["recent"]))
                {
                    if(ImGui::MenuItem(recent["name"].get<std::string>().c_str()))
                    {
                        pathToOpen = recent["path"].get<std::string>();
                        fractalToOpen = true;
                    }
                    //show path in tooltip
                    if(ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", recent["path"].get<std::string>().c_str());
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("View"))
        {

            if(ImGui::MenuItem("Show UI", "Ctrl+`", show.any))
            {
                show.any = !show.any;
            }
            if(ImGui::MenuItem("Log", "Ctrl+L", show.log))
            {
                show.log = !show.log;
            }
            if(ImGui::MenuItem("Editor", "Ctrl+E", show.editor))
            {
                show.editor = !show.editor;
            }
            if(ImGui::MenuItem("Parameters", "Ctrl+P", show.parameters))
            {
                show.parameters = !show.parameters;
            }
            if(ImGui::MenuItem("Debug Info", "Ctrl+D", show.debugInfo))
            {
                show.debugInfo = !show.debugInfo;
            }
            if(ImGui::MenuItem("Compilation status", "Ctrl+R", show.errorLog))
            {
                show.errorLog = !show.errorLog;
            }
            if(ImGui::MenuItem("Export", "Ctrl+Q", show.exportWindow))
            {
                show.exportWindow = !show.exportWindow;
            }
            if(ImGui::MenuItem("Fullscreen", "F11", show.fullscreen))
            {
                show.fullscreen = !show.fullscreen;
                vkbase::sys::fullscreen(show.fullscreen);
            }
            ImGui::EndMenu();
        }

        if(ImGui::MenuItem("Exit"))
        {
            exitFlag = true;
        }
        ImGui::EndMainMenuBar();
    }

    showMenu= ImGui::IsAnyItemFocused() || ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered() ;

}

void processShortcuts()
{
    if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftCtrl))||ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_RightCtrl))
    ||ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftCtrl))||ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightCtrl)))
    {
        //File
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_O)))
        {
            open();
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_S)))
        {
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift))||ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftShift)))//save copy as (Ctrl+Shift+S)
            {
                saveAs(true);
            }
            else if(!fractals[activeFractalIndex]->save())//save (Ctrl+S)
            {
                saveAs();
            }
        }

        if ((ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift))||ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftShift)))&&
        ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))//save all (Ctrl+Shift+A)
        {
            saveAll();
        }
        //View
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_E)))//editor (Ctrl+E)
        {
            show.editor = !show.editor;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_L))) //log (Ctrl+L)
        {
            show.log = !show.log;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_P))) //parameters (Ctrl+P)
        {
            show.parameters = !show.parameters;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_D))) //debug info (Ctrl+D)
        {
            show.debugInfo = !show.debugInfo;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_R))) //compilation status (Ctrl+R)
        {
            show.errorLog = !show.errorLog;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Q))) //export (Ctrl+Q)
        {
            show.exportWindow = !show.exportWindow;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_GraveAccent))) //show UI (Ctrl+`)
        {
            show.any = !show.any;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))) //switch tabs
        {
            ctrlTabMode = true;
            if(!lastTabs.empty())
                switchToTab=getIndexOfFractal(lastTabs[++lastTabOffset%lastTabs.size()]);
        }
        //font size
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Equal)))
        {
            vkbase::imgui::setFontSize(vkbase::imgui::getFontSize()+1);
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Minus)))
        {
            vkbase::imgui::setFontSize(vkbase::imgui::getFontSize()-1);
        }
        if(ImGui::GetIO().MouseWheel!=0)
        {
            vkbase::imgui::setFontSize(vkbase::imgui::getFontSize()+ImGui::GetIO().MouseWheel);
        }

        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftAlt)))
        {
            //remove focus from any item
            if(ImGui::IsAnyItemFocused()||ImGui::IsAnyItemActive())
                ImGui::SetWindowFocus(nullptr);
            else
                ImGui::SetWindowFocus("Code");

        }

    }
    else if(activeFractalIndex>=0)
    {
        fractals[activeFractalIndex]->processInput();
    }
    if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F11)))
    {
        show.fullscreen = !show.fullscreen;
        vkbase::sys::fullscreen(show.fullscreen);
    }
    if(ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_ReservedForModCtrl)))
    {
        ctrlTabMode = false;
        lastTabOffset = 0;
    }
}

void drawErrorLog()
{
    if(ImGui::Begin("Compilation errors", &show.errorLog))
    {
        if(!fractals.empty()&&activeFractalIndex!=-1)
        {
            if(!fractals[activeFractalIndex]->getError().empty())
                errorLog.SetText(fractals[activeFractalIndex]->getError());
            else if(fractals[activeFractalIndex]->isCompiling())
                errorLog.SetText("Compiling...");
            else
                errorLog.SetText("No compilation errors");
            errorLog.Render("errors txt");
        }
    }
    ImGui::End();
}



void ui()
{
    drawMenuBarUI();


    if(!show.any)
    {
        if(switchToTab>=0)//perform switch to tab even if UI is hidden
        {
            switchActiveFractal(switchToTab);
        }
    }
    else
    {
        //fullscreen main window over main viewport
        ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->WorkPos);
        ImGui::SetNextWindowSize(ImGui::GetWindowViewport()->WorkSize);
        if(ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                         ImGuiWindowFlags_NoBackground))
        {
            ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));

            if(ImGui::BeginTabBar("Fractals", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
            {
                int fractalToClose = -1;
                for(int i = 0; i < fractals.size(); ++i)
                {
                    bool open = true;
                    int flags = 0;

                    if(i == switchToTab)
                    {
                        flags |= ImGuiTabItemFlags_SetSelected;
                        switchToTab = -1;
                    }

                    ImGui::PushID(i);
                    if(ImGui::BeginTabItem((fractals[i]->getName() + "###").c_str(), &open, flags))
                    {
                        switchActiveFractal(i);
                        ImGui::EndTabItem();
                    }
                    //if hovered over show path
                    if(ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", fractals[i]->getSavePath().c_str());
                    }
                    ImGui::PopID();

                    if(!open)
                    {
                        fractalToClose = i;
                    }
                }


                ImGui::DockSpace(ImGui::GetID("DockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

                if(show.debugInfo)
                    vkbase::imgui::showDebugWindow();

                if(show.log)
                    vkbase::imgui::drawEngineMessagesLog();

                if(show.errorLog)
                    drawErrorLog();

                if(activeFractalIndex >= 0)
                {
                    fractals[activeFractalIndex]->ui(&show.parameters, &show.editor, &show.exportWindow);
                }

                if(fractalToClose >= 0)
                {
                    closeFractal(fractalToClose);
                }

                ImGui::EndTabBar();
            }

            ImGui::PopStyleColor();
        }
        ImGui::End();
    }

    //after everything else to be sure that input does not consumed by other windows
    processShortcuts();
}


//open fractal from main thread after open file dialog is closed
void openFractal()
{
    if(fractalToOpen)
    {
        //check if already opened
        for(int i = 0; i < fractals.size(); ++i)
        {
            try
            {
                if(std::filesystem::equivalent(fractals[i]->getSavePath(), pathToOpen))
                {
                    switchToTab = i;
                    fractalToOpen = false;
                    return;
                }
            }
            catch(const std::exception &e){}
        }

        loadFractalFromFile(pathToOpen);
        fractalToOpen = false;
    }
}


void initErrorLog()
{
    auto palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Background] = ImGui::GetColorU32(ImGuiCol_WindowBg,0.5);
    palette[(int)TextEditor::PaletteIndex::CharLiteral] = ImGui::GetColorU32(ImGuiCol_Text,0.5);
    palette[(int)TextEditor::PaletteIndex::Comment] = 0x9900ff10;
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive]=ImGui::GetColorU32(ImGuiCol_WindowBg,0.7);
    errorLog.SetPalette(palette);
    errorLog.SetReadOnly(true);
    errorLog.SetShowWhitespaces(false);
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** args)
{
    try
    {
        vkbase::init("IPTE");//init engine

        vkbase::imgui::initIMGUI();//init imgui
        vkbase::imgui::registerEngineMessagesLog();//copy engine messages to imgui log window
        NFD_Init();//init native file dialog
        initTemplates();
        initErrorLog();

        vkbase::imgui::setFontSize(16);
        ImGui::GetIO().IniFilename = nullptr;

        //load cache data
        loadAll();

        vkbase::sys::fullscreen(show.fullscreen);

        vkbase::imgui::addOnUiCallback(ui);



        //main loop
        while (vkbase::handleEvents()&&!exitFlag)
        {
            //ui();
            openFractal();
            autoSave();
            vkbase::drawFrame();
        }

        //cleanup
        saveAll();
        for(auto& fractal: fractals)
        {
            delete fractal;
        }

        NFD_Quit();//native file dialog quit
        //free engine resources
        vkbase::destroy();
    }
    catch (const std::exception &e)
    {
        std::cout<< "Exception: "+ std::string(e.what());
    }

    return EXIT_SUCCESS;
}