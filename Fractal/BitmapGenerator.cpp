#include "BitmapGenerator.hpp"
#include <stdexcept>
#include "../vkbase/core/EngineBase.h"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"
#include "../vkbase/extensions/ShadersRC/slang/ShaderCompilerSlang.hpp"

void BitmapGenerator::generateBitmap(const std::string &functionHLSL, const std::string &call, int width, int height, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout)
{
    this->width = width;
    this->height = height;
    destroy();
    auto spirv = compileShader(functionHLSL, call);

    createRenderPass();
    createPipeline(spirv);
    createImage();
    createImageView();
    createFramebuffer();
    createQueryPool();
    createCommandBuffer();


    if(descriptorPool!=VK_NULL_HANDLE && descriptorSetLayout!=VK_NULL_HANDLE)
    {
        createSampler();
        createDescriptorSet(descriptorSetLayout, descriptorPool);
    }



    submit();

}

float BitmapGenerator::getWidth() const
{
    return width;
}

float BitmapGenerator::getHeight() const
{
    return height;
}

float BitmapGenerator::getRenderTime() const
{
    return renderTime;
}

VkDescriptorSet BitmapGenerator::getDescriptorSet()
{
    return descriptorSet;
}

std::vector<uint32_t> BitmapGenerator::getBitmap()
{
    if(image==VK_NULL_HANDLE)
    {
        return {};
    }

    imageBufferHost.create(width*height*4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


    //allocate command buffer
    VkCommandBuffer cb{VK_NULL_HANDLE};

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = vkbase::commandPoolResetGraphics;
    alloc_info.commandBufferCount = 1;

    VkResult err = vkAllocateCommandBuffers(vkbase::device, &alloc_info, &cb);
    if(err!=VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffer!");
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err=vkBeginCommandBuffer(cb, &begin_info);
    if(err!=VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin command buffer!");
    }
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cb,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

        vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageBufferHost.buffer, 1, &region);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(cb,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
    }
    err=vkEndCommandBuffer(cb);
    if(err!=VK_SUCCESS)
    {
        throw std::runtime_error("failed to end command buffer!");
    }

    //submit command buffer to the queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    //pointer to an array of command buffers
    submitInfo.pCommandBuffers = &cb;

    vkQueueSubmit(vkbase::computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    //wait for the queue to finish the command buffer
    vkQueueWaitIdle(vkbase::computeQueue);
    //free command buffer
    vkFreeCommandBuffers(vkbase::device, vkbase::commandPoolResetGraphics, 1, &cb);

    auto * data = reinterpret_cast<uint32_t*> (imageBufferHost.map());
    std::vector<uint32_t> bitmap(data, data + width*height);
    imageBufferHost.unmap();
    imageBufferHost.destroy();

    return bitmap;
}

BitmapGenerator::~BitmapGenerator()
{
    destroy();
}

std::vector<uint32_t> BitmapGenerator::compileShader(const std::string &functionHLSL, const std::string &call)
{
    std::string shaderHLSL = functionHLSL + "\n" + "float4 main(float4 position ): SV_TARGET{ "
                                                   "float2 uv=(float2(position.x, -position.y)+1)/2;"
                             +"return " + call +
                             ";}";
    std::string diagnostics;
    auto spirv = vkbase::ShadersRC::compileShaderSlang(shaderHLSL,"bitmap" ,vkbase::ShadersRC::ShaderType::Fragment, "main", {}, false, diagnostics);
    return spirv;
}

void BitmapGenerator::createRenderPass()
{
    //color attachment
    VkAttachmentDescription colorAttachment{};
    //format of the attachment (swap chain image)
    colorAttachment.format = format;
    //color attachment samples (VK_SAMPLE_COUNT_1_BIT: no multisampling)
    colorAttachment.samples = samples;
    //determine what to do with the data(color and depth) in the attachment before rendering
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    //determine what to do with the data(color and depth) in the attachment after rendering
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    //what to do with the stencil data in the attachment before rendering
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    //what to do with the stencil data in the attachment after rendering
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

    //what layout the image will have before the render pass starts (VK_IMAGE_LAYOUT_UNDEFINED: don't care)
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //image layout image will have after the render pass
    //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment
    //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Images to be presented in the swap chain
    //VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: Images to be used as destination for a memory copy operation
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    //array of attachments
    std::vector<VkAttachmentDescription> attachments = {colorAttachment};

    //reference to the color attachment in the array(uses by subpass)
    VkAttachmentReference colorAttachmentRef{};
    //index of the attachment in the array
    colorAttachmentRef.attachment = 0;
    //image layout
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    //describe subpass (only one now)
    VkSubpassDescription subpass{};
    //what type of pipeline supported by this subpass
    //VK_PIPELINE_BIND_POINT_COMPUTE specifies binding as compute pipeline.
    //VK_PIPELINE_BIND_POINT_GRAPHICS specifies binding as a graphics pipeline.
    //VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR specifies binding as a ray tracing pipeline.
    //VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI specifies binding as a subpass shading pipeline.
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    //attachment references
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    //Other possible attachments that are used by subpass
    //pInputAttachments: Attachments that are read from a shader
    //pResolveAttachments: Attachments used for multisampling color attachments
    //pDepthStencilAttachment: Attachment for depth and stencil data
    //pPreserveAttachments: Attachments that are not used by this subpass, but for which the data must be preserved



    //describe dependencies between subpasses
    VkSubpassDependency dependency{};

    //is the subpass index of the first subpass in the dependency
    //(If srcSubpass is equal to VK_SUBPASS_EXTERNAL, the first synchronization scope includes commands that occur earlier in submission order than the vkCmdBeginRenderPass used to begin the render pass instance)
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;/*beginning*/

    //the subpass index of the second subpass in the dependency
    //(If dstSubpass is equal to VK_SUBPASS_EXTERNAL, the second synchronization scope includes commands that occur later in submission order than the vkCmdEndRenderPass used to end the render pass instance. )
    dependency.dstSubpass = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


    //describe render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    //array of all attachments
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.attachmentCount = attachments.size();

    //subpasses
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    //dependencies between pairs of subpasses
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    //create render pass
    if (vkCreateRenderPass(vkbase::device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void BitmapGenerator::createPipeline(const std::vector<uint32_t> &spirv)
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;

    if (vkCreatePipelineLayout(vkbase::device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    //create shader modules
    VkShaderModule vertShaderModule = vkbase::loadPrecompiledShader("shaders/VERT_shader.vert");
    VkShaderModule fragShaderModule = vkbase::createShaderModule(spirv);

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

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    //scissor rectangle
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

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
    multisampling.rasterizationSamples = samples;
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
    pipelineInfo.pDynamicState = nullptr; // Optional
    //uniform
    pipelineInfo.layout = pipelineLayout;
    //render pass describes where and how to update image
    pipelineInfo.renderPass = renderPass;
    //subpass in render pass (only one)
    pipelineInfo.subpass = 0;
    //allows to copy state from existing pipeline
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional


    //CREATE PIPELINE
    if (vkCreateGraphicsPipelines(vkbase::device,
                                  VK_NULL_HANDLE,//cache
                                  1,//count
                                  &pipelineInfo,
                                  nullptr,
                                  &pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    //modules may be destroyed now
    vkDestroyShaderModule(vkbase::device, fragShaderModule, nullptr);
    vkDestroyShaderModule(vkbase::device, vertShaderModule, nullptr);
}

void BitmapGenerator::createImage()
{
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = samples;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if(vkCreateImage(vkbase::device, &info, nullptr, &image))
    {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(vkbase::device, image, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = vkbase::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0);
    if(vkAllocateMemory(vkbase::device, &alloc_info, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }
    if(vkBindImageMemory(vkbase::device, image, imageMemory, 0))
    {
        throw std::runtime_error("failed to bind image memory!");
    }
}

void BitmapGenerator::createImageView()
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    //remapping the color channels
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    //the set of mipmap levels and array layers to be accessible to the view
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vkbase::device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image views!");
    }
}

void BitmapGenerator::createSampler()
{
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.minLod = -1000;
    sampler_info.maxLod = 1000;
    sampler_info.maxAnisotropy = 1.0f;
    if(vkCreateSampler(vkbase::device, &sampler_info, nullptr, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create sampler!");
    }
}

void BitmapGenerator::createFramebuffer()
{
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    //framebuffer is created for specific render pass
    framebufferInfo.renderPass = renderPass;
    //attach image view
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &imageView;
    //extent
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    //layers
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(vkbase::device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

void BitmapGenerator::createDescriptorSet(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool)
{
    if(descriptorSet==VK_NULL_HANDLE)
    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptorSetLayout;
        if(vkAllocateDescriptorSets(vkbase::device, &alloc_info, &descriptorSet)!=VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor set!");
        }
    }

    // Update the Descriptor Set:
    {
        VkDescriptorImageInfo desc_image[1] = {};
        desc_image[0].sampler = sampler;
        desc_image[0].imageView = imageView;
        desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write_desc[1] = {};
        write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc[0].dstSet = descriptorSet;
        write_desc[0].descriptorCount = 1;
        write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_desc[0].pImageInfo = desc_image;
        vkUpdateDescriptorSets(vkbase::device, 1, write_desc, 0, nullptr);
    }
}

void BitmapGenerator::createQueryPool()
{
    VkQueryPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.pNext = nullptr; // Optional
    createInfo.flags = 0; // Reserved for future use, must be 0!

    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = 2;

    VkResult result = vkCreateQueryPool(vkbase::device, &createInfo, nullptr, &mTimeQueryPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create time query pool!");
    }
}

void BitmapGenerator::createCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    //command pool from which command buffers will be allocated
    allocInfo.commandPool = vkbase::commandPoolResetGraphics;
    /*VK_COMMAND_BUFFER_LEVEL_PRIMARY: primary command buffers are the ones that can be submitted to a queue for execution.
    VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from the primary command buffers.*/
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    //number of command buffers to allocate
    allocInfo.commandBufferCount = 1;

    //create command buffers
    if (vkAllocateCommandBuffers(vkbase::device, &allocInfo, &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers for bitmap rendering!");
    }
    vkResetCommandBuffer(commandBuffer, 0);


    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //         VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT specifies that each recording of the command buffer will only be submitted once, and the command buffer will be reset and recorded again between each submission.
    //         VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT specifies that a secondary command buffer is considered to be entirely inside a render pass. If this is a primary command buffer, then this bit is ignored.
    //         VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT specifies that a command buffer can be resubmitted to a queue while it is in the pending state, and recorded into multiple primary command buffers.
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Optional
    // for secondary buffer only (indicates which state to inherit from calling primary command buffer)
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer for bitmap rendering!");
    }
    // record commands
    {
        //render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

        renderPassInfo.renderPass = renderPass;

        renderPassInfo.framebuffer = framebuffer;

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};


        renderPassInfo.clearValueCount=0;
        renderPassInfo.pClearValues=nullptr;

        vkCmdResetQueryPool(commandBuffer, mTimeQueryPool, 0, 2);
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, mTimeQueryPool, 0);

        //VK_SUBPASS_CONTENTS_INLINE: Render pass commands will be inlined in the primary command buffer itself, and no secondary command buffers will be executed.
        //VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: Render pass commands will be executed from secondary command buffers.
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            //bind pipeline
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            //draw
            vkCmdDraw(commandBuffer, 4, 1, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mTimeQueryPool, 1);
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void BitmapGenerator::submit()
{
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    //number of semaphores to wait on
    submitInfo.waitSemaphoreCount = 0;
    //semaphores to wait on
    submitInfo.pWaitSemaphores = nullptr;
    //wait stages
    submitInfo.pWaitDstStageMask = nullptr;
    //number of command buffers to submit
    submitInfo.commandBufferCount = 1;
    //command buffers to submit
    submitInfo.pCommandBuffers = &commandBuffer;
    //number of semaphores to signal
    submitInfo.signalSemaphoreCount = 0;
    //semaphores to signal
    submitInfo.pSignalSemaphores = nullptr;


    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    if(vkCreateFence(vkbase::device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create fence!");
    }

    if(vkQueueSubmit(vkbase::graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    //wait for the queue to finish the command buffer
    vkWaitForFences(vkbase::device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(vkbase::device, fence, nullptr);


    //retrieve render time
    uint64_t buffer[2];
    VkResult result = vkGetQueryPoolResults(vkbase::device, mTimeQueryPool, 0, 2, sizeof(uint64_t) * 2, buffer, sizeof(uint64_t),
                                            VK_QUERY_RESULT_64_BIT);
    if (result == VK_SUCCESS)
    {
        renderTime=static_cast<float>(vkbase::timestampToSeconds(buffer[1] - buffer[0]));
    }
    else
    {
        renderTime=0;
    }
}

void BitmapGenerator::destroy()
{
    vkbase::waitForRenderEnd();
    vkDestroyImageView(vkbase::device, imageView, nullptr);
    vkDestroyImage(vkbase::device, image, nullptr);
    vkFreeMemory(vkbase::device, imageMemory, nullptr);
    vkDestroySampler(vkbase::device, sampler, nullptr);
    vkDestroyFramebuffer(vkbase::device, framebuffer, nullptr);
    vkDestroyRenderPass(vkbase::device, renderPass, nullptr);
    vkDestroyPipeline(vkbase::device, pipeline, nullptr);
    vkDestroyPipelineLayout(vkbase::device, pipelineLayout, nullptr);
    vkFreeCommandBuffers(vkbase::device, vkbase::commandPoolResetGraphics, 1, &commandBuffer);
    vkDestroyQueryPool(vkbase::device, mTimeQueryPool, nullptr);

    imageView=VK_NULL_HANDLE;
    image=VK_NULL_HANDLE;
    imageMemory=VK_NULL_HANDLE;
    sampler=VK_NULL_HANDLE;
    framebuffer=VK_NULL_HANDLE;
    renderPass=VK_NULL_HANDLE;
    pipeline=VK_NULL_HANDLE;
    pipelineLayout=VK_NULL_HANDLE;
}
