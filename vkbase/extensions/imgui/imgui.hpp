#pragma once
#include "imgui/imgui.h"
#include <cstdint>
#include <vulkan/vulkan.h>

namespace vkbase::imgui
{

    void initIMGUI();

    void cleanupIMGUI();

    void createIMGUIDescriptorPool();

    void createIMGUICommandBuffers();

    void renderIMGUI();

    void rewriteIMGUICommandBuffer(uint32_t imageIndex);

    void showDebugWindow();

    VkDescriptorSetLayout getIMGUIDescriptorSetLayout();

    void dockWindow(const char* A, const char* B, float proportion);
}