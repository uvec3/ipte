#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "../vkbase/extensions/Buffer/Buffer.hpp"

class BitmapGenerator
{
    const VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT;
    int width=0;
    int height=0;

    float renderTime=0;

    VkImageView     imageView{VK_NULL_HANDLE};
    VkImage         image{VK_NULL_HANDLE};
    VkDeviceMemory imageMemory{VK_NULL_HANDLE};
    VkSampler       sampler{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};

    vkbase::Buffer  imageBufferHost;

    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};

    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};

    VkQueryPool mTimeQueryPool{VK_NULL_HANDLE};

    static constexpr auto format = VK_FORMAT_R8G8B8A8_UNORM;

public:

    void generateBitmap(const std::string& functionHLSL, const std::string& call, int width, int height,
                        VkDescriptorPool descriptorPool=VK_NULL_HANDLE, VkDescriptorSetLayout descriptorSetLayout=VK_NULL_HANDLE);

    [[nodiscard]] float getWidth() const;

    [[nodiscard]] float getHeight() const;

    [[nodiscard]] float getRenderTime() const;

    VkDescriptorSet getDescriptorSet();

    std::vector<uint32_t> getBitmap();

    ~BitmapGenerator();

private:

    std::vector<uint32_t> compileShader(const std::string& functionHLSL, const std::string& call);

    void createRenderPass();

    void createPipeline(const std::vector<uint32_t>& spirv);

    void createImage();

    void createImageView();

    void createSampler();

    void createFramebuffer();

    void createDescriptorSet(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);

    void CreateQueryPool();

    void createCommandBuffer();

    void submit();

    void destroy();
};
