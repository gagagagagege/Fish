#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>
#include <map>

#include "VulkanValidationLayers.h"

namespace Fish {

    // 如果目标队列族不存在，虽可在此抛出异常，但该函数不应决定设备可用性。
    // 例如优先选择具备独立传输队列族的设备，而非强制要求。
    // 因此需要标识指定队列族是否查找成功。
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
        // 形参名带下划线是为了不和成员 surface 撞 —— 否则初始化列表里
        // surface(surface) 看着像自引用
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

        // 引用不是拷贝 —— surface 归 TriangleApp 所有,且必须在 instance 之后、
        // 所有 device 资源之前销毁。存引用,新类就只收一个 VulkanContext* 就够。
        vk::raii::SurfaceKHR& surface;
    };
}
