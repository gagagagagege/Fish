#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include "VulkanContext.h"

#include <cstdint>
#include <vector>

namespace Fish {
	class SwapChain
	{
	public:
		SwapChain() = default;
		// physicalDevice / surface / device 从 context 取;window 只用于取 framebuffer 尺寸
		SwapChain(VulkanContext* context, GLFWwindow* window);

		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;
		SwapChain(SwapChain&&) = default;
		SwapChain& operator=(SwapChain&&) = default;

		// 原子重建。失败时保留旧的、下帧重试,不抛出去。
		void recreate();

		vk::raii::SwapchainKHR&       handle()  { return m_swapchain; }
		const std::vector<vk::Image>& images() const { return m_images; }
		vk::raii::ImageView&          imageView(uint32_t i) { return m_imageViews[i]; }
		vk::raii::Semaphore&          renderFinishedSemaphore(uint32_t i) { return m_renderFinishedSemaphores[i]; }
		vk::Format                    imageFormat() const { return m_imageFormat; }
		vk::Extent2D                  extent()      const { return m_extent; }
		uint32_t                      imageCount()  const { return static_cast<uint32_t>(m_images.size()); }

	private:
		void create(vk::raii::SwapchainKHR& out, vk::SwapchainKHR oldSwapchain);
		void createImageViews();
		void createRenderFinishedSemaphores();

		vk::raii::SwapchainKHR           m_swapchain = nullptr;
		std::vector<vk::raii::ImageView> m_imageViews;
		std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;

		// 非拥有:交换链 image 归呈现引擎,这里只是把句柄取回来,没有析构动作
		std::vector<vk::Image> m_images;
		vk::Format             m_imageFormat = vk::Format::eUndefined;
		vk::Extent2D           m_extent{};

		VulkanContext*  m_context = nullptr;
		GLFWwindow* m_window  = nullptr;
	};

}
