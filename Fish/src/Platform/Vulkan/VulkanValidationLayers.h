#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <vector>

#ifdef DEBUG
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;
#endif

namespace Fish {

    //验证层
    bool checkValidationLayerSupport(const vk::raii::Context& context, const std::vector<const char*>& validationLayers);
    bool checkValiLayerInAvilableLayer(const std::vector<const char*>& validationLayers,
        const std::vector<vk::LayerProperties>& availableLayers);

    std::vector<const char*> getRequiredExtensions();

    //调试回调函数
    VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo);
    void setupDebugMessenger(vk::raii::Instance& instance, vk::raii::DebugUtilsMessengerEXT& debugMessenger);

}
