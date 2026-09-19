#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include "VulkanValidationLayers.h"
#include "VulkanContext.h"
#include "VulkanSurface.h"
#include "VulkanSwapChain.h"
#include "VulkanPipeline.h"
#include "VulkanShader.h"
#include "VulkanCommandPool.h"
#include "VulkanDescriptorAllocator.h"
#include "VulkanFrameData.h"
#include "VulkanTexture.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <optional>

#include <vector>
#include <map>
#include <memory>


namespace Fish {

	class TriangleApp {
	public:
		TriangleApp();
		void run();

	private:
		GLFWwindow* window = nullptr;

		vk::raii::Context context;
		vk::raii::Instance instance = nullptr;

		const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
		vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

		const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

		vk::raii::SurfaceKHR surface = nullptr;

		//instance 之后、其他 device 资源之前声明
		std::unique_ptr<VulkanContext> deviceClass;

		// 交换链 + image + view + present 信号量,一个对象管完。
		// 重建边界就是这四样的边界,见 SwapChain::recreate。
		SwapChain swapChain;

		std::vector<vk::DynamicState> dynamicStates = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		DescriptorAllocator descriptorAllocator;

		// pipelineLayout 归 Pipeline 自己持有
		Pipeline graphicsPipeline;

		CommandPool commandPool;
		CommandPool transientPool;

		bool framebufferResized = false;

		const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

		static const uint32_t WIDTH = 800;
		static const uint32_t HEIGHT = 600;

		const std::vector<Vertex> vertices = {
			{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f}},
			{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 2.0f}},
			{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {2.0f, 2.0f}}
		};

		const std::vector<uint16_t> indices = {
			0, 1, 2, 2, 3, 0
		};

		Buffer vertexBuffer;
		Buffer indexBuffer;

		Frames frames;

		texture mainTexture;

	private:
		void initWindow();
		void initVulkan();
		void mainLoop();
		void cleanUp();
		void createInstance();

		void drawFrame();
		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	};

}
