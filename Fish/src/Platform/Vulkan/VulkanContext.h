#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>
#include <map>

#include "VulkanValidationLayers.h"

namespace Fish {
    class QueueFamilyIndices {
    public:
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

    public:
        inline bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
        static QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface);
    };



    class VulkanContext {
    public:
        VulkanContext(vk::raii::Instance& instance, vk::raii::SurfaceKHR& surface_, const std::vector<const char*>& deviceExtensions, const std::vector<const char*>& validationLayers);

        vk::raii::PhysicalDevice pickPhysicalDevice(vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface, const std::vector<const char*>& deviceExtensions);
        static int determinePhysicalDeviceScore(const vk::raii::PhysicalDevice& device);
        static bool isDeviceSuitable(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface, const std::vector<const char*>& deviceExtensions);

        void createLogicalDevice(const std::vector<const char*>& validationLayers, const vk::raii::SurfaceKHR& surface, const std::vector<const char*>& deviceExtensions);

        static bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device, const std::vector<const char*>& deviceExtensions);

    public:
        vk::raii::PhysicalDevice physicalDevice = nullptr;
        vk::raii::Device device = nullptr;
        vk::raii::Queue graphicsQueue = nullptr;
        vk::raii::Queue presentQueue = nullptr;

        vk::raii::SurfaceKHR& surface;
    };
}
