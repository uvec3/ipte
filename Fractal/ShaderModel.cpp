#include <fstream>
#include <iostream>
#include <cctype>
#include "ShaderModel.hpp"
#include "HLSLCompiler.h"
#include <spirv_reflect.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <variant>
#include <shaderc/shaderc.hpp>
#include <boost/algorithm/string.hpp>

#include "../cmake-build-debug-visual-studio/_deps/boost-src/libs/algorithm/include/boost/algorithm/string/case_conv.hpp"
#include "../vkbase/extensions/Assets/assets.h"
#include "../vkbase/extensions/ShadersRC/slang/ShaderCompilerSlang.hpp"


ShaderModel::ShaderModel(std::string name) : name(std::move(name))
{
    currentCompiler = &hlsl_compiler;
    keysBuffer = vkbase::Buffer(sizeof(keys), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

    createDescriptorSetLayout2();
    recreateSurfaceDependentObjects();
    createComputeFence();
    createComputeCommandBuffer();


    updateCommandBuffers = true;
    OnPrepareReceiver::disable();
}


void ShaderModel::setSource(const std::string& src)
{
    m_source = src;
    recompileFlag = true;
}

const std::string& ShaderModel::getSource() const
{
    return m_source;
}

void ShaderModel::recreateParametersBuffers()
{
    uniformBuffers.clear();

    if (pipeline.pipelineLayout != VK_NULL_HANDLE)
    {
        for (int i = 0; i < vkbase::imageCount; ++i)
        {
            uniformBuffers.push_back(uniformParameters.createBuffers(VK_BUFFER_USAGE_2_TRANSFER_DST_BIT));
            std::cout << uniformBuffers[i].buffers[0].isDeviceLocal() << "---\n";
            UniformParameters::updateDescriptorSet(descriptorSets[i], uniformBuffers[i]);
        }
    }
}

void ShaderModel::recreateSurfaceDependentObjects()
{
    vkDestroyDescriptorPool(vkbase::device, descriptorPoolStatic, nullptr);
    vkDestroyDescriptorPool(vkbase::device, descriptorPoolDynamic, nullptr);
    vkDestroyCommandPool(vkbase::device, commandPool, nullptr);


    createDescriptorPools();
    createDescriptorSets2();

    createDescriptorSets();
    recreateParametersBuffers();
    createCommandPool();
    createRenderBuffers();
}



void ShaderModel::recompile()
{
    if (!recompileFlag)
        return;
    recompileFlag = false;

    using namespace std::chrono;
    auto start_time = high_resolution_clock::now();
    slang_project.updateDependencies(m_source);

    status = COMPILING;
    auto paths = slang_project.getRoot().empty()
                     ? std::vector<std::string>{}
                     : std::vector{slang_project.getRoot().string()};

    compilationTaskManager.runTask([
                                       name=name,
                                       src=m_source,
                                       paths=paths,
                                       currentCompiler=currentCompiler,
                                       setLayout2=descriptorSet2Layout,
                                       root_path=slang_project.getRoot().generic_string(),
                                       parameters=uniformParameters]
                               (std::stop_token stop_token) mutable
                                   {
                                       std::cout << "+Compilation started:" << name << "\n";


                                       PipelineData pipeline_data{};
                                       vkbase::rc::CompilationResult frag_result{};
                                       vkbase::rc::CompilationResult compute_result{};


                                       std::thread t([&]()
                                       {
                                           compute_result = currentCompiler->compileCompute(src, name, root_path);
                                       });

                                       frag_result = currentCompiler->compile(src, name, root_path);

                                       t.join();

                                       if (stop_token.stop_requested())
                                       {
                                           std::cout << "-Compilation cancelled:" << name << " \n";
                                           return std::tuple{
                                               frag_result, compute_result, std::move(pipeline_data),
                                               std::move(parameters)
                                           };
                                       }


                                       if (frag_result.success && compute_result.success)
                                       {
                                           try
                                           {
                                               parameters = UniformParameters(parameters, frag_result.spirv);
                                               auto set_layout = parameters.createDescriptorSetLayout();
                                               pipeline_data = PipelineData(
                                                   frag_result.spirv, compute_result.spirv, set_layout,
                                                   setLayout2->get());
                                           }
                                           catch (std::exception& e)
                                           {
                                               std::cout << "Pipeline creation failed: " << e.what() << "\n";
                                               frag_result.success = false;
                                               compute_result.success = false;
                                           }
                                       }
                                       std::cout << "-Compilation finished:" << name << " \n";
                                       return std::tuple{
                                           frag_result, compute_result, std::move(pipeline_data), std::move(parameters)
                                       };
                                   },
                                   [=,this](auto&& result)
                                   {
                                       auto& [frag_result,compute_result,pipeline_data,new_parameters] = result;

                                       compilation_time = (high_resolution_clock::now() - start_time).count() /
                                           1000000000.0;
                                       fragment_shader = std::move(frag_result);
                                       compute_shader = std::move(compute_result);
                                       isChanged = true;

                                       if (frag_result.success && compute_result.success && pipeline_data.isValid())
                                       {
                                           vkQueueWaitIdle(vkbase::graphicsQueue);
                                           pipeline = std::move(pipeline_data);
                                           uniformParameters = new_parameters;
                                           syncCompilation();
                                           std::cout << "=Binary code updated:" << name << "\n";
                                           status = COMPILED;
                                       }
                                       else
                                       {
                                           status = ERROR;
                                       }
                                   });
}

void ShaderModel::syncCompilation()
{
    if (!fragment_shader.spirv.empty())
    {
        vkDeviceWaitIdle(vkbase::device);

        createDescriptorSets();
        recreateParametersBuffers();
        UniformParameters::updateDescriptorSet(computeDescriptorSets, *uniformParameters.localBuffers);
        updateCommandBuffers = true;
    }
}

nlohmann::json ShaderModel::toJson()
{
    nlohmann::json json;
    json["name"] = name;
    json["languageName"] = currentCompiler->languageName;
    json["source"] = m_source;
    json["parameters"] = uniformParameters;
    return json;
}

void ShaderModel::loadFromJson(const nlohmann::json& json)
{
    name = json.at("name");
    if (json.contains(language_name))
        language_name = json.at("languageName");
    else
        language_name = "HLSL";
    if (json.contains("source"))
        m_source = json.at("source");
    else
        m_source = json.at("compilers").at(0).at("source");

    if (language_name == "HLSL")
        currentCompiler = &hlsl_compiler;
    else
        throw std::runtime_error("Unsupported shader language: " + language_name);

    if (json.contains("parameters"))
        from_json(json["parameters"], uniformParameters);
    recompileFlag = true;
}

void ShaderModel::setViewArea(glm::vec4 newArea)
{
    if (newArea != viewArea)
    {
        viewArea = newArea;
        updateCommandBuffers = true;
    }
}

AbstractShaderCompiler& ShaderModel::getCurrentCompiler()
{
    return *currentCompiler;
}

const glm::vec4& ShaderModel::getViewArea()
{
    return viewArea;
}

void ShaderModel::setActive(bool active)
{
    if (active && writeMainBufferCallbackId == -1)
    {
        writeMainBufferCallbackId = vkbase::addWriteMainBufferCallback(WRAP_MEMBER_FUNC(writeCommandBuffer));
        prepareCallbackId = vkbase::addDrawPrepareCallback(WRAP_MEMBER_FUNC(prepare), 10);
        vkbase::OnPrepareReceiver::enable();
    }
    else if (!active && writeMainBufferCallbackId != -1)
    {
        vkbase::removeWriteMainBufferCallback(writeMainBufferCallbackId);
        vkbase::removeDrawPrepareCallback(prepareCallbackId);
        vkbase::OnPrepareReceiver::disable();
        writeMainBufferCallbackId = -1;
        prepareCallbackId = -1;
    }
}


VkDescriptorSetLayout ShaderModel::createGraphicsDescriptorSetLayout()
{
    VkDescriptorSetLayout descriptorSetLayout;
    //binding id within shader
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    //allowed stages
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    //descriptor set
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vkbase::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
    return descriptorSetLayout;
}

VkDescriptorSetLayout ShaderModel::createComputeDescriptorSetLayout()
{
    VkDescriptorSetLayout descriptorSetLayout;
    //COMPUTE PIPELINE
    VkDescriptorSetLayoutBinding computeLayoutBindings[]
    {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        }
    };

    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    computeLayoutInfo.bindingCount = std::size(computeLayoutBindings);
    computeLayoutInfo.pBindings = computeLayoutBindings;

    if (vkCreateDescriptorSetLayout(vkbase::device, &computeLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout for compute pipeline!");
    }
    return descriptorSetLayout;
}

void ShaderModel::createDescriptorSetLayout2()
{
    VkDescriptorSetLayoutBinding bindings[]
    {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = std::size(bindings);
    layoutInfo.pBindings = bindings;


    auto ds = vk::Device(vkbase::device).createDescriptorSetLayoutUnique(layoutInfo);
    descriptorSet2Layout = std::make_shared<vk::UniqueDescriptorSetLayout>(std::move(ds));
}

void ShaderModel::createDescriptorSets2()
{
    descriptorSets2.resize(vkbase::imageCount);
    std::vector<VkDescriptorSetLayout> layouts(vkbase::imageCount, descriptorSet2Layout->get());

    VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPoolStatic;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(vkbase::imageCount);
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(vkbase::device, &allocInfo,
                                 descriptorSets2.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets 2!");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = keysBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = keysBuffer.size();

    std::vector<VkWriteDescriptorSet> writes(vkbase::imageCount);
    for (int i = 0; i < vkbase::imageCount; ++i)
    {
        VkWriteDescriptorSet descriptorWrite{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descriptorWrite.dstSet = descriptorSets2[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional
        writes[i] = descriptorWrite;
    }

    vkUpdateDescriptorSets(vkbase::device, writes.size(), writes.data(), 0, nullptr);
}

void ShaderModel::createDescriptorPools()
{
    VkDescriptorPoolSize poolSizes[]{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1 + vkbase::imageCount + 10},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1}
    };

    VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = static_cast<uint32_t>(vkbase::imageCount + 1 + 1);

    if (vkCreateDescriptorPool(vkbase::device, &poolInfo, nullptr, &descriptorPoolDynamic) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    VkDescriptorPoolSize poolSizesStatic[]{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = vkbase::imageCount},
    };

    VkDescriptorPoolCreateInfo poolInfoStatic{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfoStatic.poolSizeCount = std::size(poolSizesStatic);
    poolInfoStatic.pPoolSizes = poolSizesStatic;
    poolInfoStatic.maxSets = static_cast<uint32_t>(vkbase::imageCount);

    if (vkCreateDescriptorPool(vkbase::device, &poolInfoStatic, nullptr, &descriptorPoolStatic) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}


void ShaderModel::createDescriptorSets()
{
    if (pipeline.pipelineLayout != VK_NULL_HANDLE)
    {
        vkResetDescriptorPool(vkbase::device, descriptorPoolDynamic, 0);
        //GRAPHICS PIPELINE
        //resize the array of descriptor on image count
        descriptorSets.resize(vkbase::imageCount);

        std::vector<VkDescriptorSetLayout> layouts(vkbase::imageCount, pipeline.descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPoolDynamic;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(vkbase::imageCount);
        allocInfo.pSetLayouts = layouts.data();


        //allocate an array of descriptor sets
        //the descriptor sets will be automatically freed with descriptor pool
        if (vkAllocateDescriptorSets(vkbase::device, &allocInfo,
                                     descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        //COMPUTE PIPELINE
        VkDescriptorSetAllocateInfo comp_allocInfo{};
        comp_allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        comp_allocInfo.descriptorPool = descriptorPoolDynamic;
        comp_allocInfo.descriptorSetCount = 1;
        comp_allocInfo.pSetLayouts = &pipeline.descriptorSetLayout;

        if (vkAllocateDescriptorSets(vkbase::device, &comp_allocInfo,
                                     &computeDescriptorSets) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets for compute pipeline!");
        }
    }
}

void ShaderModel::createRenderBuffers()
{
    //command buffers for each frame
    cbRender.resize(vkbase::imageCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    //command pool from which command buffers will be allocated
    allocInfo.commandPool = commandPool;
    /*VK_COMMAND_BUFFER_LEVEL_PRIMARY: primary command buffers are the ones that can be submitted to a queue for execution.
    VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from the primary command buffers.*/
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    //number of command buffers to allocate
    allocInfo.commandBufferCount = (uint32_t)cbRender.size();

    //create command buffers
    if (vkAllocateCommandBuffers(vkbase::device, &allocInfo, cbRender.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    //    for (size_t i = 0; i < cbRender.size(); i++)
    //    {
    //        //create empty command buffers
    //        VkCommandBufferBeginInfo beginInfo{};
    //        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    //        auto inheritanceInfo =vkbase::createMainBufferInheritanceInfo(i);
    //        beginInfo.pInheritanceInfo = &inheritanceInfo;
    //
    //        if (vkBeginCommandBuffer(cbRender[i], &beginInfo) != VK_SUCCESS)
    //            throw std::runtime_error("failed to begin recording command buffer!");
    //
    //        vkEndCommandBuffer(cbRender[i]);
    //    }
}

void ShaderModel::createComputeCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkbase::commandPoolResetGraphics;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(vkbase::device, &allocInfo, &cbCompute) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;

    if (vkBeginCommandBuffer(cbCompute, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording compute command buffer!");
    }
    vkEndCommandBuffer(cbCompute);
}

void ShaderModel::rewriteRenderCommands(int i)
{
    VkCommandBuffer cb = cbRender[i];
    VkCommandBufferInheritanceInfo inheritanceInfo = vkbase::createMainBufferInheritanceInfo(i);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //         VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT specifies that each recording of the command buffer will only be submitted once, and the command buffer will be reset and recorded again between each submission.
    //         VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT specifies that a secondary command buffer is considered to be entirely inside a render pass. If this is a primary command buffer, then this bit is ignored.
    //         VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT specifies that a command buffer can be resubmitted to a queue while it is in the pending state, and recorded into multiple primary command buffers.
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT; // Optional
    // for secondary buffer only (indicates which state to inherit from calling primary command buffer)
    beginInfo.pInheritanceInfo = &inheritanceInfo; // Optional

    if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    //render commands
    {
        //render area(pipeline uses dynamic state)
        VkViewport viewport{};
        viewport.x = viewArea.x * static_cast<float>(vkbase::extent.width);
        viewport.y = viewArea.y * static_cast<float>(vkbase::extent.height);
        viewport.width = static_cast<float>(vkbase::extent.width) * viewArea.z;
        viewport.height = static_cast<float>(vkbase::extent.height) * viewArea.w;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cb, 0, 1, &viewport);
        //scissor rectangle
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vkbase::extent;
        vkCmdSetScissor(cb, 0, 1, &scissor);
        //bind descriptor set
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline.pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline.pipelineLayout, 1, 1, &descriptorSets2[i], 0, nullptr);
        //bind pipeline
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);
        //draw rectangle
        vkCmdDraw(cb, 3/*vertex count*/, 1/*instance count*/,
                  0/*offset gl_VertexIndex*/, 0/*(gl_InstanceIndex)*/);
    }
    vkEndCommandBuffer(cb);
}

void ShaderModel::rewriteComputeBuffer()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;

    if (vkBeginCommandBuffer(cbCompute, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    {
        vkCmdBindPipeline(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.computePipeline);
        vkCmdBindDescriptorSets(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipelineLayout, 0, 1,
                                &computeDescriptorSets, 0, nullptr);
        vkCmdBindDescriptorSets(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipelineLayout, 1, 1,
                                &descriptorSets2[0], 0, nullptr);
        vkCmdDispatch(cbCompute, 1, 1, 1);
    }

    vkEndCommandBuffer(cbCompute);
}

void ShaderModel::createComputeFence()
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;

    if (vkCreateFence(vkbase::device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create fence!");
    }
}

void ShaderModel::submitComputeBuffer()
{
    if (compute_shader.spirv.empty())
        return;

    //copy data
    // uniformParameters.uploadBuffers();

    //copy keys
    memcpy(keysBuffer.map(), keys, sizeof(keys));
    keysBuffer.unmap();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cbCompute;

    vkQueueSubmit(vkbase::computeQueue, 1, &submitInfo, computeFence);

    vkWaitForFences(vkbase::device, 1, &computeFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vkbase::device, 1, &computeFence);


    //copy data back
    // uniformParameters.downloadBuffers();
}

void ShaderModel::rewriteAllCommandBuffers()
{
    if (pipeline.graphicsPipeline && pipeline.computePipeline)
    {
        vkDeviceWaitIdle(vkbase::device);
        vkResetCommandPool(vkbase::device, commandPool, 0);
        for (int i = 0; i < cbRender.size(); i++)
        {
            rewriteRenderCommands(i);
        }

        rewriteComputeBuffer();
        cbRenderValid = true;
    }
    else
    {
        cbRenderValid = false;
    }
}


VkPipelineLayout ShaderModel::createGraphicsPipelineLayout(const VkDescriptorSetLayout& descriptorSetLayout,
                                                           const VkDescriptorSetLayout& descriptorSet2Layout)
{
    VkDescriptorSetLayout setLayouts[]
    {
        descriptorSetLayout,
        descriptorSet2Layout
    };

    VkPipelineLayout layout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //count descriptor sets layout
    pipelineLayoutInfo.setLayoutCount = std::size(setLayouts);
    //array of descriptor set layout's
    pipelineLayoutInfo.pSetLayouts = setLayouts; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(vkbase::device, &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    return layout;
}

VkPipelineLayout ShaderModel::createComputePipelineLayout(const VkDescriptorSetLayout& descriptorSetLayout,
                                                          VkDescriptorSetLayout descriptorSet2Layout)
{
    VkDescriptorSetLayout setLayouts[]
    {
        descriptorSetLayout,
        descriptorSet2Layout
    };

    VkPipelineLayout layout;
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = std::size(setLayouts);
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts;

    if (vkCreatePipelineLayout(vkbase::device, &pipelineLayoutCreateInfo, nullptr, &layout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    return layout;
}

void ShaderModel::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    //queue which will be used to submit command buffers to
    poolInfo.queueFamilyIndex = vkbase::queueFamilyIndices.graphicsFamily.value();
    //possible flags:
    //VK_COMMAND_POOL_CREATE_TRANSIENT_BIT specifies that command buffers allocated from the pool will be short-lived, meaning that they will be reset or freed in a relatively short timeframe. This flag may be used by the implementation to control memory allocation behavior within the pool.
    //VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from a pool to be individually reset to the initial state; either by calling vkResetCommandBuffer, or via the implicit reset when calling vkBeginCommandBuffer. If this flag is not set on a pool, then vkResetCommandBuffer must not be called for any command buffer allocated from that pool.
    //VK_COMMAND_POOL_CREATE_PROTECTED_BIT specifies that command buffers allocated from the pool are protected command buffers.
    poolInfo.flags = 0;

    //create poll (command poll will be destroyed in cleanup function)
    if (vkCreateCommandPool(vkbase::device, &poolInfo, nullptr, &commandPool/*out handle*/) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool!");
}

VkPipeline ShaderModel::createGraphicsPipeline(const std::vector<uint32_t>& frag_shader, VkPipelineLayout layout)
{
    //create shader modules
    VkShaderModule vertShaderModule = vkbase::createShaderModule(vkbase::assets::get("shaders/vert.spv"));
    VkShaderModule fragShaderModule = vkbase::createShaderModule(frag_shader);


    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";


    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};


    //vertex input (empty)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;


    //Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    /*Topology, one of: VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP*/
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;


    //--VIEWPORT--//
    //viewport and scissor (dynamic state)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    //--RASTERIZATION--//
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    //depth basis
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional


    //--MULTISAMPLING--//(disabled)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional


    //--BLENDING--//
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional


    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional


    //--DYNAMIC STATES--//
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = std::size(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;


    //--PIPELINE--//
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    //shaders
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    //vertex input
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    //draw mode
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    //viewport (dynamic)
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState; // Optional
    //uniform
    pipelineInfo.layout = layout;
    //render pass describes where and how to update image
    pipelineInfo.renderPass = vkbase::renderPass;
    //subpass in render pass (only one)
    pipelineInfo.subpass = 0;
    //allows to copy state from existing pipeline
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    //CREATE PIPELINE
    VkPipeline newPipeline;
    auto result = vkCreateGraphicsPipelines(vkbase::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                            &newPipeline);

    //destroy shader modules
    vkDestroyShaderModule(vkbase::device, fragShaderModule, nullptr);
    vkDestroyShaderModule(vkbase::device, vertShaderModule, nullptr);

    if (result != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }


    return newPipeline;
}

VkPipeline ShaderModel::createComputePipeline(const std::vector<uint32_t>& comp_shader, VkPipelineLayout layout)
{
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = vkbase::createShaderModule(comp_shader);
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = layout;
    pipelineInfo.flags = 0;

    VkPipeline newPipeline;
    auto result = vkCreateComputePipelines(vkbase::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);

    //destroy shader module
    vkDestroyShaderModule(vkbase::device, pipelineInfo.stage.module, nullptr);

    if (result != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return newPipeline;
}

void ShaderModel::onSurfaceChanged()
{
    recreateSurfaceDependentObjects();
    syncCompilation();
    updateCommandBuffers = true;
}

void ShaderModel::prepare([[maybe_unused]] uint32_t image_index)
{
    compilationTaskManager.finish();
    if (!slang_project.modifiedFiles.empty())
    {
        recompileFlag = true;
        slang_project.modifiedFiles.clear();
        slang_project.updateDependencies(m_source);
    }

    recompile();

    if (updateCommandBuffers)
    {
        rewriteAllCommandBuffers();
        updateCommandBuffers = false;
    }

    submitComputeBuffer();
}

void ShaderModel::onUpdateData(uint32_t imageIndex)
{
    //updateUniform(imageIndex);
}

void ShaderModel::onWriteBeforeRenderBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    if (cbRenderValid)
    {
        ParametersBuffers::moveBuffersCMD(commandBuffer, *uniformParameters.localBuffers, uniformBuffers[imageIndex]);
    }
}

void ShaderModel::writeCommandBuffer(VkCommandBuffer cbMain, uint32_t imageIndex)
{
    if (cbRenderValid)
    {
        vkCmdExecuteCommands(cbMain, 1, &cbRender[imageIndex]);
    }
}


void ShaderModel::destroy()
{
    setActive(false);
    vkDeviceWaitIdle(vkbase::device);
    vkDestroyDescriptorPool(vkbase::device, descriptorPoolDynamic, nullptr);
    vkDestroyDescriptorPool(vkbase::device, descriptorPoolStatic, nullptr);
    vkDestroyFence(vkbase::device, computeFence, nullptr);

    vkDestroyCommandPool(vkbase::device, commandPool, nullptr);
}

ShaderModel::~ShaderModel()
{
    destroy();
    compilationTaskManager.detachAll();
}



std::string getTypeFromReflection(const nlohmann::json& reflection, const std::string& parent, std::span<std::string> path)
{
    if (reflection.contains("types"))
    {
        auto types=reflection["types"];
        if (types.contains(parent))
        {
            for (const auto& member:types[parent]["members"])
            {
                if (member.at("name")==path[0])
                {
                    if (path.size()==1)
                    {
                        std::string type=member.at("type");
                        if (types.contains(type))
                        {
                            return types[type].at("name");
                        }
                        return type;
                    }
                    return getTypeFromReflection(reflection,member["type"],std::span(path.begin()+1,path.end()));
                }
            }
        }
    }
    return "";
}

std::string getTypeFromReflection(const nlohmann::json reflection, int binding, int set, std::span<std::string> path)
{
    if (reflection.contains("ssbos"))
    {
        for (auto member: reflection["ssbos"])
        {
            if (member["binding"] == binding && member["set"] == set)
            {
                return getTypeFromReflection(reflection, member["type"], std::span<std::string>(path.begin() + 1, path.end()));
            }
        }
    }

    if (reflection.contains("ubos"))
    {
        for (auto member: reflection["ubos"])
        {
            if (member["binding"] == binding && member["set"] == set)
            {
                return getTypeFromReflection(reflection, member["type"], std::span<std::string>(path.begin() + 1, path.end()));
            }
        }
    }

    return "";
}

struct DynamicParameter
{
    std::vector<std::string> path;
    std::string glsl_type;
    std::string dotName;
    std::string underscoreName;
    std::string type_name;
    bool is_used=false;

    DynamicParameter(int binding,const std::string& name,const nlohmann::json& reflection,const std::string& suffix="_0")
    {
        boost::algorithm::split(path, name, boost::is_any_of("."));
        for (auto &part : path)
        {
            underscoreName+=part+"_";
        }
        underscoreName.pop_back();

        for (auto &part : path)
        {
            part+=suffix;
        }

        for (auto &part : path)
        {
            dotName+=part+".";
        }
        dotName.pop_back();


        type_name=getTypeFromReflection(reflection,binding,0,path);
        if (type_name.empty())
        {
            throw std:: runtime_error("Failed to init dynamic parameter "+name+" because it doesn't exists!");
        }

    }
};


std::map<int,std::tuple<std::string,std::string>> retrieveGLSLInputs(const std::string & glsl_code)
{
    std::map<int,std::tuple<std::string,std::string>> inputs;
    boost::regex re(
        R"((?:layout\s*\([^)]*\blocation\s*=\s*(\d+)[^)]*\)\s*)?(?:(?:flat|smooth|noperspective|centroid|sample|invariant|precise|highp|mediump|lowp|readonly|writeonly|coherent|volatile|restrict)\s+)*in\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)(?:\s*\[[^\]]*\])?\s*;)"
    );
    boost::sregex_iterator it(glsl_code.begin(), glsl_code.end(), re);
    boost::sregex_iterator end;
    while (it != end)
    {
        int location = (*it)[1].matched ? std::stoi((*it)[1].str()) : static_cast<int>(inputs.size());
        std::string type = (*it)[2].str();
        std::string name = (*it)[3].str();
        inputs[location] = {type, name};
        ++it;
    }

    return inputs;
}

std::map<int,std::tuple<std::string,std::string>> retrieveHLSLInputs(const std::string & glsl_code)
{
    std::map<int,std::tuple<std::string,std::string>> inputs;
    //step 1: find struct SPIRV_Cross_Input
    boost::regex struct_re(R"(struct\s+SPIRV_Cross_Input\s*\{([^}]*)\})");
    boost::sregex_iterator struct_it(glsl_code.begin(), glsl_code.end(), struct_re);
    boost::sregex_iterator end;
    if (struct_it == end)
    {
        throw std::runtime_error("Failed to find SPIRV_Cross_Input struct in GLSL code");
    }
    std::string struct_body = (*struct_it)[1].str();

    //step 2: find members with semantic
    boost::regex member_re(
        R"((?:\s*(?:nointerpolation|linear|centroid|noperspective|sample|flat|smooth|precise|highp|mediump|lowp|readonly|writeonly|coherent|volatile|restrict)\s+)*([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)(\d*)\s*(?:\[[^\]]*\])?\s*;)"
    );
    for (boost::sregex_iterator it(struct_body.begin(), struct_body.end(), member_re); it != end; ++it)
    {
        std::string type = (*it)[1].str();
        std::string name = (*it)[2].str();
        std::string semantic = (*it)[3].str();
        std::string semantic_index = (*it)[4].str();

        int location = -1;
        if (!semantic_index.empty())
        {
            location = std::stoi(semantic_index);
        }
        else
        {
            // Fallback: extract trailing digits from semantic name, otherwise append by order.
            size_t digit_start = semantic.find_last_not_of("0123456789");
            if (digit_start != std::string::npos && digit_start + 1 < semantic.size())
            {
                location = std::stoi(semantic.substr(digit_start + 1));
            }
        }

        if (location < 0)
        {
            location = static_cast<int>(inputs.size());
        }

        inputs[location] = {type, name};
    }

    return inputs;
}



ExportResult export_internal(vkbase::rc::CompilationResult standardized_glsl,
                                const std::string dstFuncName,
                             const std::string& language,
                             std::string params,
                             const std::vector<std::tuple<int, std::string>>& dynamic_parameters)
{
    ExportResult result{};

    result.debug += "STANDARDIZED GLSL:\n\n";
    result.debug += standardized_glsl.text;
    result.debug += "\n";
    result.debug += standardized_glsl.diagnostics_text;

    if (!standardized_glsl.success)
        return result;

    std::string standardized_glsl_text = std::move(standardized_glsl.text);

    vkbase::rc::CompilationParameters info;
    info.compile_debug = true;
    info.source = standardized_glsl_text;
    info.type = vkbase::rc::ShaderType::Fragment;
    info.source_is_glsl = true;
    auto standardized_spirv = vkbase::rc::compile_glsl(info);

    spirv_cross::CompilerReflection compiler(standardized_spirv);
    auto standardized_reflection = nlohmann::json::parse(compiler.compile());


    result.debug += "\n\nSTANDARDIZED SPIRV REFLECTION:\n\n";
    result.debug += standardized_reflection.dump(4);


    std::vector<DynamicParameter> additionalDynamicParameters;

    try
    {
        for (const auto& param : dynamic_parameters)
        {
            additionalDynamicParameters.emplace_back(std::get<0>(param), std::get<1>(param), standardized_reflection);
        }
    }
    catch (const std::exception& e)
    {
        result.debug += "\n\nError while processing dynamic parameters: \n";
        result.debug += e.what();
        return result;
    }

    boost::regex bindings(
        R"__(layout\s*\([^)]*\)\s*(?:buffer|uniform)\s+([A-Za-z_]\w*)\s*\{\s*([^{}]+?)\s*\}\s*([A-Za-z_]\w*)(\s*\[[^\]]*\])?\s*;)__"
    );

    // Collect binding instance names before replacement
    std::vector<std::string> bindingNames;
    for (boost::sregex_iterator it(standardized_glsl_text.begin(), standardized_glsl_text.end(),
                                   bindings), end; it != end; ++it)
    {
        // $3 is the instance name
        std::string name = (*it)[3].str();

        // Optional: keep array suffix if you need it
        if ((*it)[4].matched)
        {
            name += (*it)[4].str();
        }

        bindingNames.push_back(name);
    }


    //filter out unused params
    std::string filtered_params;
    std::vector<std::string> lines;
    boost::split(lines, params, boost::is_any_of("\n"));
    for (const auto& line : lines)
    {
        bool used = false;
        for (const auto& pname : bindingNames)
        {
            if (line.starts_with(pname + "."))
            {
                used = true;
                break;
            }
        }

        bool dynamic = false;
        for (auto& dp : additionalDynamicParameters)
        {
            if (line.starts_with(dp.dotName))
            {
                if (line[dp.dotName.length()] == ' ' || line[dp.dotName.length()] == '.')
                    dynamic = true;
                dp.is_used = true;
                break;
            }
        }


        if (used && !dynamic)
        {
            filtered_params += line + "\n";
        }
    }


    std::string additional_parameters_string;
    {
        int inLocation = 0;
        if (standardized_reflection.contains("inputs"))
            inLocation = standardized_reflection["inputs"].size();

        for (const auto& param : additionalDynamicParameters)
        {
            //if (param.is_used)
            {
                additional_parameters_string += "\nflat in " +
                    param.type_name + " " + param.underscoreName + ";";
            }
        }
    }

    for (const auto& param : additionalDynamicParameters)
    {
        if (param.is_used)
            filtered_params += param.dotName + "=" + param.underscoreName + ";\n";
    }


    {
        boost::regex main = boost::regex(R"(\bvoid\s+main\s*\(\s*\)\s*\{())");
        //inset params
        standardized_glsl_text = boost::regex_replace(
            standardized_glsl_text, main, additional_parameters_string + "\nvoid main("")\n{\n" + filtered_params);
    }

    // Now replace
    standardized_glsl_text = boost::regex_replace(
        standardized_glsl_text, bindings, "struct $1 { $2 } $3;");


    //remove all locations for in
    boost::regex input_location(
        R"(layout\s*\(\s*[^)]*\blocation\s*=\s*\d+\s*[^)]*\)\s*(?=(?:\w+\s+)*in\b))"
    );
    standardized_glsl_text = boost::regex_replace(standardized_glsl_text, input_location, "");

    result.debug += "STANDARDIZED GLSL FINAL:\n\n";
    result.debug += standardized_glsl_text;

    vkbase::rc::CompilationParameters info2;
    info2.compile_debug = true;
    info2.source = standardized_glsl_text;
    info2.type = vkbase::rc::ShaderType::Fragment;
    info2.source_is_glsl = true;
    auto final_spirv = vkbase::rc::compile_glsl(info2);
    final_spirv = vkbase::rc::optimize_spirv(final_spirv);

    std::string output;
    if (final_spirv.empty())
        return result;

    spirv_cross::CompilerReflection compilerReflection(final_spirv);
    nlohmann::json final_reflection = nlohmann::json::parse(compilerReflection.compile());

    std::cout<<final_reflection.dump(4);

    std::string shaderCode;
    if (language == "GLSL")
    {
        spirv_cross::CompilerGLSL compilerGLSL(final_spirv);
        spirv_cross::CompilerGLSL::Options options;
        options.version = 460;
        compilerGLSL.set_common_options(options);
        shaderCode = compilerGLSL.compile();
    }
    else if (language == "HLSL")
    {
        spirv_cross::CompilerHLSL compilerHLSL(final_spirv);
        spirv_cross::CompilerHLSL::Options options;
        options.shader_model = 60;
        compilerHLSL.set_hlsl_options(options);
        shaderCode = compilerHLSL.compile();
    }

    std::cout << shaderCode;

    //find entry point
    std::string entryPointPrefix;
    if (language == "GLSL")
        entryPointPrefix = "void main()\n{";
    else if (language == "HLSL")
        entryPointPrefix = "void frag_main()\n{";
    auto beginPos = shaderCode.find(entryPointPrefix);
    if (beginPos == std::string::npos)
    {
        throw std::runtime_error("Entry point not found!");
    }

    auto endPos = beginPos + entryPointPrefix.size();
    int bracketCount = 1;
    for (; endPos < shaderCode.size(); endPos++)
    {
        if (shaderCode[endPos] == '{')
            bracketCount++;
        else if (shaderCode[endPos] == '}')
            bracketCount--;
        if (bracketCount == 0)
            break;
    }
    if (endPos == shaderCode.size())
    {
        throw std::runtime_error("Entry point not found!");
    }

    beginPos += entryPointPrefix.size();


    std::string type = final_reflection["outputs"][0]["type"].get<std::string>();
    std::string out_name = final_reflection["outputs"][0]["name"].get<std::string>();
    boost::replace_all(out_name, ".", "_");
    std::string entryPointOutput = out_name + " = ";
    //replace . with _


    //get constants
    std::string constPrefix;
    if (language == "GLSL")
        constPrefix = "const";
    else if (language == "HLSL")
        constPrefix = "static";
    std::string constants;
    size_t i = 0;
    while ((i = shaderCode.find(constPrefix, i)) != std::string::npos)
    {
        auto end = shaderCode.find(';', i);

        //check if not constant
        if (shaderCode.substr(i + constPrefix.size(), 6) != " const")
        {
            auto lastSpace = shaderCode.rfind(' ', end);
            std::string varName = shaderCode.substr(lastSpace + 1, end - lastSpace - 1);

            bool skip = false;
            for (auto p : final_reflection["inputs"]) //skip if input parameter
            {
                if (p["name"].get<std::string>() == varName)
                {
                    skip = true;
                    break;
                }
            }
            if (skip)
            {
                i = end;
                continue;
            }
            std::cout << "var_name: " << varName << std::endl;
        }

        std::string globalVar = shaderCode.substr(i, end - i + 1);
        if (!globalVar.ends_with(out_name + ";"))
            constants += "\n    " + globalVar;
        i = end;
    }


    std::string funcBody = shaderCode.substr(beginPos, endPos - beginPos);


    auto it = funcBody.find(entryPointOutput);
    if (it != std::string::npos)
        funcBody.replace(it, entryPointOutput.size(), "return ");
    else
    {
        result.debug+= "\nFailed to export function: output variable not found\n";
        return result;
    }


    if (language == "HLSL")
        type = glslToHlsl(type);
    std::string funcHeader = type + " " + dstFuncName + "(";




    std::map<int,std::tuple<std::string,std::string>> inputs;
    if (language=="GLSL")
        inputs = retrieveGLSLInputs(shaderCode);
    else if (language=="HLSL")
        inputs = retrieveHLSLInputs(shaderCode);


    for (auto in : inputs|std::views::values)
        funcHeader += std::get<0>(in) +" "+std::get<1>(in) + ",";

    if (!inputs.empty())
        funcHeader.pop_back();


    funcHeader += ")\n{";


    std::string func = funcHeader + constants + funcBody;
    func += "}";

    result.code=func;
    result.success=true;

    return result;
}


void ShaderModel::exportOutput(const std::string& srcFuncName, const std::string& dstFuncName,
                               const std::string& language,
                               const std::vector<std::tuple<int, std::string>>& dynamic_parameters,
                               std::move_only_function<void(ExportResult)> callback)
{
    std::string params = uniformParameters.initC("_0");

    compilationTaskManager.runTask([
                                       language,
                                       params,
                                       dynamic_parameters,
                                       name=name,
                                       src=m_source,
                                       currentCompiler=currentCompiler,
                                       root_path=slang_project.getRoot().generic_string(), srcFuncName,dstFuncName]
    (const std::stop_token& stop_token) mutable
    {

        std::cout << "+Export started:" << name << "\n";

        auto standardized_glsl = currentCompiler->compileForExport(src, name, srcFuncName, root_path);
        return export_internal(std::move(standardized_glsl),dstFuncName,language,params,dynamic_parameters);
    },
    std::move(callback));
}

void ShaderModel::generateBitmap(int width, int height, const std::string& functionName,
                                 const std::string& functionCall, VkDescriptorSetLayout descriptorSetLayout)
{
    // auto hlslExportFunction = exportOutput(functionName, "HLSL", TODO);
    // bitmapGenerator.generateBitmap(hlslExportFunction, functionCall, width, height, descriptorPoolDynamic, descriptorSetLayout);
}

VkDescriptorSet ShaderModel::getBitmapDescriptorSet()
{
    return bitmapGenerator.getDescriptorSet();
}

std::vector<uint32_t> ShaderModel::getBitmap()
{
    return bitmapGenerator.getBitmap();
}

glm::vec2 ShaderModel::getBitmapSize()
{
    return {bitmapGenerator.getWidth(), bitmapGenerator.getHeight()};
}

const BitmapGenerator& ShaderModel::getBitmapGenerator()
{
    return bitmapGenerator;
}

const std::string& ShaderModel::get_error()
{
    return fragment_shader.diagnostics_text;
}

const vkbase::rc::CompilationResult ShaderModel::get_frag_result() const
{
    return fragment_shader;
}

const vkbase::rc::CompilationResult ShaderModel::get_compute_result() const
{
    return compute_shader;
}

bool ShaderModel::isCompiling() const
{
    return !compilationTaskManager.isFree();
}
