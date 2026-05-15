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
#include "HLSLCompiler.h"
#include "../External/json.hpp"
#include "BitmapGenerator.hpp"
#include "Project.hpp"
#include "../vkbase/core/ParallelTaskManager.hpp"

std::string glslToHlsl(const std::string &glslT);

class UniformParameter
{
public:
    std::string name;
    std::string type;
    int offset=0;
    std::vector<char> data;
    bool isDynamic = false;
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
    std::string dynamicParametersString();
};



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
    std::string m_source;
    std::string name;
    std::string language_name;
    UniformParameters uniformParameters;
    float keys[512]{0};
    Project slang_project;
    double compilation_time=0;

private:

    struct PipelineData
    {
        VkPipeline graphicsPipeline{VK_NULL_HANDLE};
        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline computePipeline{VK_NULL_HANDLE};
        VkPipelineLayout computePipelineLayout{VK_NULL_HANDLE};
        VkDescriptorSetLayout computeDescriptorSetLayout{VK_NULL_HANDLE};


        [[nodiscard]] bool isValid() const
        {
            return graphicsPipeline != VK_NULL_HANDLE && computePipeline != VK_NULL_HANDLE;
        }

        PipelineData()=default;

        PipelineData(const std::vector<uint32_t>& graphics_spv,const std::vector<uint32_t>& compute_spv,VkDescriptorSetLayout set2Layout):PipelineData()
        {
            computeDescriptorSetLayout=createComputeDescriptorSetLayout();
            computePipelineLayout=createComputePipelineLayout(computeDescriptorSetLayout, set2Layout);
            computePipeline=createComputePipeline(compute_spv,computePipelineLayout);
            if (!computePipeline)
            {
                destroy();
                return;
            }

            descriptorSetLayout=createGraphicsDescriptorSetLayout();
            pipelineLayout=createGraphicsPipelineLayout(descriptorSetLayout, set2Layout);
            graphicsPipeline=createGraphicsPipeline(graphics_spv,pipelineLayout);
            if (!graphicsPipeline)
            {
                destroy();
            }
        }
        PipelineData(const PipelineData&)=delete;
        PipelineData& operator =(const PipelineData&)=delete;
        ~PipelineData(){destroy();}
        PipelineData (PipelineData&& other)
        {
            *this=std::move(other);
        }

        PipelineData& operator=(PipelineData&& other)
        {
            destroy();
            graphicsPipeline=other.graphicsPipeline;
            descriptorSetLayout=other.descriptorSetLayout;
            pipelineLayout=other.pipelineLayout;
            computePipeline=other.computePipeline;
            computePipelineLayout=other.computePipelineLayout;
            computeDescriptorSetLayout=other.computeDescriptorSetLayout;
            std::memset(static_cast<void*>(&other),0,sizeof(other));
            return  *this;
        }

    private:
        void destroy()
        {
            vkDestroyDescriptorSetLayout(vkbase::device, descriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(vkbase::device, pipelineLayout, nullptr);
            vkDestroyPipeline(vkbase::device, graphicsPipeline, nullptr);
            vkDestroyPipelineLayout(vkbase::device, computePipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(vkbase::device, computeDescriptorSetLayout, nullptr);
            vkDestroyPipeline(vkbase::device, computePipeline, nullptr);
            std::memset(static_cast<void*>(this),0,sizeof(*this));
        }
    };

    //Vulkan objects
    PipelineData pipeline{};
    VkDescriptorPool descriptorPoolDynamic{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPoolStatic{VK_NULL_HANDLE};
    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<vkbase::Buffer> uniformBuffers;
    std::vector<VkCommandBuffer> cbRender  {VK_NULL_HANDLE};
    bool cbRenderValid = false;
    bool updateCommandBuffers = false;

    VkDescriptorSetLayout descriptorSet2Layout;
    std::vector<VkDescriptorSet> descriptorSets2;

    vkbase::Buffer parametersIntermediateBuffer;
    vkbase::Buffer keysBuffer;
    VkDescriptorSet computeDescriptorSets{VK_NULL_HANDLE};
    VkCommandBuffer cbCompute{VK_NULL_HANDLE};
    VkFence computeFence{VK_NULL_HANDLE};
    BitmapGenerator bitmapGenerator;

    //LOGIC
    glm::vec4 viewArea{0.0,0.0,1,1};
    bool switched = false;
    bool recompileFlag=false;
    int writeMainBufferCallbackId = -1;
    int prepareCallbackId = -1;
    int updateExtentCallbackId = -1;
    std::atomic<bool> isChanged = false;
    std::unique_ptr<AbstractShaderCompiler> currentCompiler = nullptr;
    vkbase::ShadersRC::CompilationResult fragment_shader;
    vkbase::ShadersRC::CompilationResult compute_shader;
    std::string newUniformParametersReflection;


    ParallelTaskManager compilationTaskManager{1,1,true};


public:
    explicit ShaderModel(std::string name);
    ~ShaderModel();

    void recompile();
    void setSource(const std::string &source);
    const std::string& getSource() const;
    void recreateSurfaceDependentObjects();

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
    const std::string& get_error();
    const vkbase::ShadersRC::CompilationResult get_frag_result() const;
    const vkbase::ShadersRC::CompilationResult get_compute_result() const;
    bool isCompiling() const;

private:
    static VkDescriptorSetLayout createGraphicsDescriptorSetLayout();
    static VkDescriptorSetLayout createComputeDescriptorSetLayout();

    void createDescriptorSetLayout2();
    void createDescriptorSets2();
    void createDescriptorPools();
    void createUniformBuffer();
    void updateDescriptorSet(uint32_t i);
    void updateComputeDescriptorSet();
    void createDescriptorSets();
    void createRenderBuffers();
    void createComputeCommandBuffer();
    void rewriteRenderCommands(int i);
    void rewriteComputeBuffer();
    void createComputeFence();
    void rewriteAllCommandBuffers();
    static VkPipelineLayout createGraphicsPipelineLayout(const VkDescriptorSetLayout& descriptorSetLayout, const VkDescriptorSetLayout& descriptorSet2Layout);
    static VkPipelineLayout createComputePipelineLayout(const VkDescriptorSetLayout& descriptorSetLayout, VkDescriptorSetLayout descriptorSet2Layout);
    void createCommandPool();
    static VkPipeline createGraphicsPipeline(const std::vector<uint32_t>& frag_shader,VkPipelineLayout layout);
    static VkPipeline createComputePipeline(const std::vector<uint32_t>& comp_shader, VkPipelineLayout layout);

private:
    void onSurfaceChanged() override;
    void prepare([[maybe_unused]] [[maybe_unused]] [[maybe_unused]] uint32_t image_index);
    void onUpdateData(uint32_t imageIndex) override;
    void writeCommandBuffer(VkCommandBuffer cbMain, uint32_t imageIndex);

    void updateUniform(uint32_t imageIndex);
    void syncCompilation();
    void submitComputeBuffer();
    void destroy();
};