#include "VulkanValidationLayers.h"

#include <iostream>
#include <stdexcept>
#include <cstring>

namespace Fish {

    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo = {};
        createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                     vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                     vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                 vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                 vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        createInfo.pfnUserCallback = debugCallback;
    }

    bool checkValidationLayerSupport(const vk::raii::Context& context, const std::vector<const char*>& validationLayers)
    {
        auto availableLayers = context.enumerateInstanceLayerProperties();

        return checkValiLayerInAvilableLayer(validationLayers, availableLayers);
    }

    bool checkValiLayerInAvilableLayer(const std::vector<const char*>& validationLayers,
        const std::vector<vk::LayerProperties>& availableLayers)
    {
        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT messageType, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return vk::False;
    }

    void setupDebugMessenger(vk::raii::Instance& instance, vk::raii::DebugUtilsMessengerEXT& debugMessenger)
    {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
    }

}
