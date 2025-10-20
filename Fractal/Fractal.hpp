#pragma once
#include <vector>
#include <memory>
#include "ShaderModel.hpp"
#include "ShaderEditor.hpp"
#include "FileWatcher.hpp"
#include "../vkbase/core/EngineBase.h"
#include <queue>

template<typename T>
struct ParallelTask
{
    std::thread thread;
    std::function<void(T)> finisher;
    T data;
    std::atomic<bool> finished{false};
};

template<typename T>
class ParallelTaskManager
{
protected:
    int maxThreads=1;
    int acceptThreads=1;
    std::atomic<int> runningThreads=0;
    std::deque<ParallelTask<T>> tasks;

public:
    explicit ParallelTaskManager(int maxThreads=1, int acceptThreads=1):maxThreads(maxThreads),acceptThreads(acceptThreads)
    {
        if(acceptThreads>maxThreads)
            throw std::runtime_error("acceptThreads>maxThreads");
    }

    virtual void runTask(std::function<T()> task, std::function<void(T)> taskFinisher)
    {
        if(runningThreads<maxThreads)
        {
            if(tasks.size()==acceptThreads)
            {
                tasks.back().thread.detach();
                tasks.pop_back();
            }

            tasks.emplace_front();
            ++runningThreads;
            ParallelTask<T>& currentTask = tasks.front();
            std::thread t([task,&currentTask,this]()
                          {
                              currentTask.data = task();
                              currentTask.finished=true;
                              --runningThreads;
                          }
            );

            currentTask.finisher=taskFinisher;
            currentTask.thread=std::move(t);
            currentTask.finished=false;
        }
    }

    void finish()
    {
        while(!tasks.empty()&&tasks.back().finished)
        {
            tasks.back().thread.join();
            tasks.back().finisher(tasks.back().data);
            tasks.pop_back();
        }
    }

    ~ParallelTaskManager()
    {
        for(auto& task:tasks)
        {
            task.thread.detach();
        }
    }
};

template<typename T>
class AutoParallelTaskManager:public ParallelTaskManager<T>, public vkbase::OnLogicUpdateReceiver
{
public:
    explicit AutoParallelTaskManager(int maxThreads=1, int acceptThreads=1, int priority=0):ParallelTaskManager<T>(maxThreads,acceptThreads)
    {
        vkbase::OnLogicUpdateReceiver::disable();
        vkbase::OnLogicUpdateReceiver::setPriority(priority);
    }

    void runTask(std::function<T()> task, std::function<void(T)> taskFinisher) override
    {
        ParallelTaskManager<T>::runTask(task, taskFinisher);
        if(this->tasks.size()>0)
            vkbase::OnLogicUpdateReceiver::enable();
    }

    void onUpdateLogic(uint32_t imageIndex) override
    {
        this->finish();
        if(this->tasks.size()==0)
            vkbase::OnLogicUpdateReceiver::disable();
    }
};

class Fractal:public vkbase::OnLogicUpdateReceiver
{
    bool saved= false;
    std::string savePath;
    std::unique_ptr<ShaderModel> shaderModel;
    bool ignoreNextMirrorFileChange=false;
    std::string fileForMirroring;
    std::atomic<bool> mirrorToFileFlag=false;
    bool savedToMirrorFile=false;
    std::chrono::time_point<std::chrono::system_clock> lastSaveTime;
    bool newError=false;

    ShaderEditor* shaderEditor;
    TextEditor exportResult;
    ParallelTaskManager<std::string> exportParallelTaskManager{1,1};
    AutoParallelTaskManager<std::pair<std::string,std::string>> imageSaveTaskManager{1,1};
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

    void mirrorToFile(const std::string &filename,bool initFromFile=true);
    void removeMirrorFile();
    const std::string& getMirrorFile();

    void setName(std::string newName);
    void setActive(bool active);

    const std::string& getName();
    bool isSaved() const;
    const std::string& getSavePath() const;
    void initHLSL();
    std::string getError();
    bool isCompiling();
    bool hasCompilationError();
    void setRenderArea(float x, float y, float w, float h);

private:
    virtual void onUpdateLogic(uint32_t imageIndex) override;

    void autoSaveToMirrorFile();
    void createMirrorFile();
    void writeMirrorFile();
    void readFromMirrorFile();
    void mirrorFileChanged(FileAction action);
    void sourceChanged();

    void drawEditor(bool* open=nullptr);
    void parametersUi(bool *showParameters);
    void exportWindow(bool *show);
};
