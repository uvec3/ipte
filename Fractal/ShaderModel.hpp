#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vector>
#include <atomic>
#include <cstdint>
#include <thread>
#include <memory>
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
#include "Parameters.hpp"
#include "../vkbase/core/ParallelTaskManager.hpp"
#include "Parameters.hpp"

struct ExportResult
{
    bool success=false;
    std::string code;
    std::string debug;
};

class ShaderModel: public vkbase::OnPrepareReceiver,public vkbase::OnSurfaceChangedReceiver,public vkbase::OnWriteBeforeRenderBufferReceiver
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
        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline graphicsPipeline{VK_NULL_HANDLE};
        VkPipeline computePipeline{VK_NULL_HANDLE};

        [[nodiscard]] bool isValid() const
        {
            return graphicsPipeline != VK_NULL_HANDLE && computePipeline != VK_NULL_HANDLE;
        }

        PipelineData()=default;

        PipelineData(const std::vector<uint32_t>& graphics_spv,const std::vector<uint32_t>& compute_spv,VkDescriptorSetLayout setLayout,VkDescriptorSetLayout set2Layout):PipelineData()
        {
            descriptorSetLayout=setLayout;
            pipelineLayout=createGraphicsPipelineLayout(descriptorSetLayout, set2Layout);
            graphicsPipeline=createGraphicsPipeline(graphics_spv,pipelineLayout);
            if (!graphicsPipeline)
            {
                destroy();
                return;
            }

            computePipeline=createComputePipeline(compute_spv,pipelineLayout);
            if (!computePipeline)
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
            std::memset(static_cast<void*>(&other),0,sizeof(other));
            return  *this;
        }

    private:
        void destroy()
        {
            vkDestroyDescriptorSetLayout(vkbase::device, descriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(vkbase::device, pipelineLayout, nullptr);
            vkDestroyPipeline(vkbase::device, graphicsPipeline, nullptr);
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
    std::vector<ParametersBuffers> uniformBuffers;
    std::vector<VkCommandBuffer> cbRender  {VK_NULL_HANDLE};
    bool cbRenderValid = false;
    bool updateCommandBuffers = false;

    std::shared_ptr<vk::UniqueDescriptorSetLayout> descriptorSet2Layout;
    std::vector<VkDescriptorSet> descriptorSets2;

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
    AbstractShaderCompiler* currentCompiler = nullptr;
    vkbase::rc::CompilationResult fragment_shader;
    vkbase::rc::CompilationResult compute_shader;


    vkbase::ParallelTaskManager compilationTaskManager{3,1,true};


public:
    explicit ShaderModel(std::string name);
    ~ShaderModel();

    void recompile();
    void setSource(const std::string &source);
    const std::string& getSource() const;
    void recreateParametersBuffers();
    void recreateSurfaceDependentObjects();

    void setViewArea(glm::vec4 newArea);
    const glm::vec4& getViewArea();
    AbstractShaderCompiler& getCurrentCompiler();
    void setActive(bool active);
    nlohmann::json toJson();
    void loadFromJson(const nlohmann::json &json);


    void exportOutput(const std::string& srcFuncName, const std::string& dstFuncName,
                      const std::string& language, const std::vector<std::tuple<int, std::string>>& dynamic_parameters,
                      std::
                      move_only_function<void(ExportResult)>
                      callback);
    void generateBitmap(int width, int height, const std::string& functionName, const std::string &functionCall, VkDescriptorSetLayout descriptorSetLayout= nullptr);
    VkDescriptorSet getBitmapDescriptorSet();
    glm::vec2 getBitmapSize();
    std::vector<uint32_t> getBitmap();
    const BitmapGenerator& getBitmapGenerator();
    const std::string& get_error();
    const vkbase::rc::CompilationResult get_frag_result() const;
    const vkbase::rc::CompilationResult get_compute_result() const;
    bool isCompiling() const;

private:
    static VkDescriptorSetLayout createGraphicsDescriptorSetLayout();
    static VkDescriptorSetLayout createComputeDescriptorSetLayout();

    void createDescriptorSetLayout2();
    void createDescriptorSets2();
    void createDescriptorPools();
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


protected:

    void onSurfaceChanged() override;
    void prepare([[maybe_unused]] [[maybe_unused]] [[maybe_unused]] uint32_t image_index);
    void onUpdateData(uint32_t imageIndex) override;
    void onWriteBeforeRenderBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) override;
    void writeCommandBuffer(VkCommandBuffer cbMain, uint32_t imageIndex);


    void syncCompilation();
    void submitComputeBuffer();
    void destroy();
};
