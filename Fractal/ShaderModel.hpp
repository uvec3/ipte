#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <vector>
#include <atomic>
#include <cstdint>
#include <thread>
#include "AbstractShaderCompiler.h"
#include <vulkan/vulkan.h>
#include "../vkbase/core/EngineBase.h"
#include "../vkbase/extensions/imgui/imgui.hpp"
#include "../vkbase/extensions/imgui/Log.hpp"
#include "../vkbase/extensions/Buffer/Buffer.hpp"
#include "SPIRVCompiler.h"
#include "HLSLCompiler.h"
#include "../External/json.hpp"
#include "BitmapGenerator.hpp"


class UniformParameter
{
public:
    std::string name;
    std::string type;
    int offset=0;
    std::vector<char> data;
    nlohmann::json metadata;

    template<typename T>
    T& get()
    {
        return *reinterpret_cast<T*>(data.data());
    }

    int size() const
    {
        return static_cast<int>(data.size());
    }
};

class UniformParameters
{
public:
    std::map<std::string, UniformParameter> activeParameters;
    std::map<std::string, UniformParameter> removedParameters;
    int size=0;

    UniformParameters()=default;
    UniformParameters (const UniformParameters& other)=default;
    UniformParameters& operator=(UniformParameters const& other)=default;
    UniformParameters(UniformParameters oldParameters, const std::string &reflection);
    std::vector<char> buildBuffer();
    void readFromBuffer(void* buffer);
    nlohmann::json serialize();
    void deserialize(nlohmann::json j);
    std::string initStructureString(const std::string& varName);
};

std::string exportHLSL(std::vector<uint32_t> spirv);

class ShaderModel: public vkbase::OnDataUpdateReceiver,public vkbase::OnSurfaceChangedReceiver
{
public:
    enum Status
    {
        NONE,
        ERROR,
        COMPILING,
        COMPILED
    };

public:
    //public data
    std::atomic<Status> status = NONE;
    std::string errorMessage;
    std::vector<std::unique_ptr<AbstractShaderCompiler>> compilers;
    std::string name;
    UniformParameters uniformParameters;
    float keys[512]{0};

private:
    //Vulkan objects
    VkPipeline graphicsPipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<vkbase::Buffer> uniformBuffers;
    std::vector<VkCommandBuffer> cbRender{VK_NULL_HANDLE};
    bool cbRenderValid = false;
    bool updateBuffers = false;
    bool recomplile = false;

    vkbase::Buffer parametersIntermediateBuffer;
    vkbase::Buffer keysBuffer;
    VkPipeline computePipeline{VK_NULL_HANDLE};
    VkPipelineLayout computePipelineLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout computeDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet computeDescriptorSets{VK_NULL_HANDLE};
    VkCommandBuffer cbCompute{VK_NULL_HANDLE};
    VkFence computeFence{VK_NULL_HANDLE};
    BitmapGenerator bitmapGenerator;

    //LOGIC
    glm::vec4 viewArea{0.0,0.0,1,1};
    bool switched = false;
    int writeMainBufferCallbackId = -1;
    int prepareCallbackId = -1;
    int updateExtentCallbackId = -1;
    std::atomic<bool> isChanged = false;
    AbstractShaderCompiler *currentCompiler = nullptr;
    std::vector<uint32_t> shaderBin;
    std::vector<uint32_t> computeShaderBin;
    std::string newUniformParametersReflection;

    //Synchronization
    std::atomic<int> threadsCount = 0;
    std::atomic<int> lastThreadID = 0;
    std::mutex shaderMutex;

    //how many compilation threads can be run simultaneously
    static constexpr int MAX_THREADS = 2;

public:
    explicit ShaderModel(std::string name);
    ~ShaderModel();

    void setSource(const std::string &source);
    std::string getSource();
    void updateBin();
    [[maybe_unused]] void updateTranslation(AbstractShaderCompiler* compiler);
    AbstractShaderCompiler* getCompilerByLanguage(const std::string& languageName);
    void setCurrentCompiler(const std::string& languageName);
    void setViewArea(glm::vec4 newArea);
    const glm::vec4& getViewArea();
    AbstractShaderCompiler& getCurrentCompiler();
    void setActive(bool active);
    nlohmann::json toJson();
    void loadFromJson(const nlohmann::json &json);
    std::string exportOutput(const std::string& funcName, const std::string& language,bool onlyBody);
    void generateBitmap(int width, int height, const std::string& functionName, const std::string &functionCall, VkDescriptorSetLayout descriptorSetLayout= nullptr);
    VkDescriptorSet getBitmapDescriptorSet();
    glm::vec2 getBitmapSize();
    std::vector<uint32_t> getBitmap();
    const BitmapGenerator& getBitmapGenerator();

private:
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createUniformBuffer();
    void updateDescriptorSet(uint32_t i);
    void updateComputeDescriptorSet();
    void createDescriptorSets();
    void createRenderBuffers();
    void createComputeCommandBuffer();
    void rewriteRenderBuffer(int i);
    void rewriteComputeBuffer();
    void createComputeFence();
    void rewriteAllCommandBuffers();
    void createPipelineLayout();
    void createCommandPool();
    void createGraphicsPipeline(const std::vector<uint32_t>& frag_shader);
    void createComputePipeline(const std::vector<uint32_t>& comp_shader);

private:
    void onSurfaceChanged() override;
    void prepare([[maybe_unused]] [[maybe_unused]] [[maybe_unused]] uint32_t image_index);
    void onUpdateData(uint32_t imageIndex) override;
    void writeCommandBuffer(VkCommandBuffer cbMain, uint32_t imageIndex);

    void updateUniform(uint32_t imageIndex);
    void syncShaderCode();
    void submitComputeBuffer();
    void destroy();
};