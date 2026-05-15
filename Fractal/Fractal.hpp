#pragma once
#include <vector>
#include <memory>
#include "ShaderModel.hpp"
#include "ShaderEditor.hpp"
#include "FileWatcher.hpp"
#include "../vkbase/core/EngineBase.h"



class Fractal
{
    bool saved= false;
    std::string savePath;
    std::unique_ptr<ShaderModel> shaderModel;
    bool newError=false;

    ShaderEditor* shaderEditor;
    TextEditor exportResult;
    ParallelTaskManager exportParallelTaskManager{1,1,true};
    AutoParallelTaskManager imageSaveTaskManager{1,1};
    bool exportCompiling=false;


    struct Cache
    {
        int bmpWidth=512;
        int bmpHeight=512;
        char callBmpExport[1024]{"generated(uv)"};
        int exportType=0;
        bool exportOnlyBody=false;
    };

    Cache cache;

public:
    Fractal();

    //Forbid copying
    Fractal(const Fractal&)= delete;
    Fractal& operator=(const Fractal&)= delete;
    Fractal(Fractal&&)= delete;

    void ui(bool* showParameters= nullptr, bool* showEditor=nullptr, bool* showExport=nullptr);
    void processInput();

    bool save();
    bool save(const std::string &path, bool updatePath=true);
    bool load(const std::string &path);
    nlohmann::json serialize();
    void deserialize(const nlohmann::json& j);

    void setName(std::string newName);
    void setActive(bool active);

    const std::string& getName();
    bool isSaved() const;
    const std::string& getSavePath() const;
    void initHLSL();
    const std::string& getError();
    bool isCompiling();
    bool hasCompilationError();
    void setRenderArea(float x, float y, float w, float h);

private:
    void sourceChanged();

    void drawEditor(bool* open=nullptr);
    void parametersUi(bool *showParameters);
    void exportWindow(bool *show);
};
