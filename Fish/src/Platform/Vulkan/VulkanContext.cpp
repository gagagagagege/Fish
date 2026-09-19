#include "VulkanContext.h"

#include <set>

#include <stdexcept>
#include <iostream>

namespace Fish {
    QueueFamilyIndices QueueFamilyIndices::findQueueFamilies(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) {
        QueueFamilyIndices indices;

        auto queueFamilies = device.getQueueFamilyProperties();

        int i = 0;

        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                indices.graphicsFamily = i;
            }
            if (device.getSurfaceSupportKHR(i, *surface)) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) {
                break;
            }
            i++;
        }
        return indices;
    }

    VulkanContext::VulkanContext(vk::raii::Instance& instance, vk::raii::SurfaceKHR& surface_, const std::vector<const char*>& deviceExtensions, const std::vector<const char*>& validationLayers)
        : surface(surface_)
    {
        physicalDevice = pickPhysicalDevice(instance, surface, deviceExtensions);
        createLogicalDevice(validationLayers, surface, deviceExtensions);
    }

    //物理设备
    vk::raii::PhysicalDevice VulkanContext::pickPhysicalDevice(vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface, const std::vector<const char*>& deviceExtensions)
    {
        auto devices = instance.enumeratePhysicalDevices();
        if (devices.empty()) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::multimap<int, vk::raii::PhysicalDevice> candidates;
        for (auto& physicalDevice : devices) {
            if (isDeviceSuitable(physicalDevice, surface, deviceExtensions)) {
                int score = determinePhysicalDeviceScore(physicalDevice);
                candidates.emplace(score, std::move(physicalDevice));
            }
        }
        if (candidates.rbegin()->first > 0) {
            return std::move(candidates.rbegin()->second);
        }
        else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    int VulkanContext::determinePhysicalDeviceScore(const vk::raii::PhysicalDevice& device)
    {
        auto deviceProperties = device.getProperties();
        auto deviceFeatures = device.getFeatures();
        int score = 0;

        // Discrete GPUs have a significant performance advantage
        if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            score += 1000;
        }

        // Maximum possible size of textures affects graphics quality
        score += deviceProperties.limits.maxImageDimension2D;

        // Application can't function without geometry shaders
        if (!deviceFeatures.geometryShader) {
            return 0;
        }

        return score;
    }

    bool VulkanContext::isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface, const std::vector<const char*>& deviceExtensions)
    {
        // Check if the physicalDevice supports the Vulkan 1.3 API version
        bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

        // Check if any of the queue families support both graphics and presentation to our surface
        auto     queueFamilies = physicalDevice.getQueueFamilyProperties();
        uint32_t qfpIndex = 0;
        bool     supportsGraphicsAndPresent =
            std::ranges::any_of(queueFamilies,
                [&physicalDevice, &surface = surface, &qfpIndex](auto const& qfp) {
                    bool const suitable = (qfp.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface);
                    qfpIndex++;
                    return suitable;
                });

        // Check if all required physicalDevice extensions are available
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions =
            std::ranges::all_of(deviceExtensions,
                [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
                    return std::ranges::any_of(availableDeviceExtensions,
                        [requiredDeviceExtension](auto const& availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
                });

        // Check if the physicalDevice supports the required features
        auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        // Return true if the physicalDevice meets all the criteria
        return supportsVulkan1_3 && supportsGraphicsAndPresent && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    void VulkanContext::createLogicalDevice(const std::vector<const char*>& validationLayers, const vk::raii::SurfaceKHR& surface, const std::vector<const char*>& deviceExtensions)
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        uint32_t                              queueIndex = ~0;

        // get the first index into queueFamilyProperties which supports both graphics and present
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
            {
                // found a queue family that supports both graphics and present
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == ~0)
        {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        // query for Vulkan 1.3 features
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {.features = {.samplerAnisotropy = true}},                   // vk::PhysicalDeviceFeatures2
            {.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
            {.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        };

        // create a logical device
        float                     queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{ .queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority };
        vk::DeviceCreateInfo      deviceCreateInfo{ .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                                   .queueCreateInfoCount = 1,
                                                   .pQueueCreateInfos = &deviceQueueCreateInfo,
                                                   .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                                                   .ppEnabledExtensionNames = deviceExtensions.data() };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
        presentQueue  = vk::raii::Queue(device, queueIndex, 0);
    }

    bool VulkanContext::checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device, const std::vector<const char*>& deviceExtensions)
    {
        auto availableExtensions = device.enumerateDeviceExtensionProperties();

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

}
