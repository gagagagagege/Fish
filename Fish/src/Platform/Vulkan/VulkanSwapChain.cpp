#include "VulkanSwapChain.h"

#include "VulkanImage.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Fish {
	// 四个纯查询:只吃参数、不吃成员,所以放文件里,不进头文件。
	namespace {
		struct Support {
			std::vector<vk::SurfaceFormatKHR> formats;
			std::vector<vk::PresentModeKHR>   presentModes;
			vk::SurfaceCapabilitiesKHR        capabilities;
		};

		Support querySupport(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface)
		{
			return { device.getSurfaceFormatsKHR(*surface),
					 device.getSurfacePresentModesKHR(*surface),
					 device.getSurfaceCapabilitiesKHR(*surface) };
		}

		vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
		{
			for (const auto& availableFormat : availableFormats) {
				if (availableFormat.format == vk::Format::eB8G8R8A8Srgb
					&& availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
					return availableFormat;
				}
			}
			return availableFormats[0];
		}

		vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
		{
			for (const auto& availablePresentMode : availablePresentModes) {
				if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
					return availablePresentMode;
				}
			}
			return vk::PresentModeKHR::eFifo;
		}

		vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
		{
			if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
				return capabilities.currentExtent;
			}

			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			vk::Extent2D actualExtent{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	SwapChain::SwapChain(VulkanContext* context, GLFWwindow* window)
		: m_context(context), m_window(window)
	{
		create(m_swapchain, nullptr);
		createImageViews();
		createRenderFinishedSemaphores();
	}

	// 建到调用方给的句柄上,不直接写 m_swapchain ——
	// recreate 传的是临时对象,这里抛异常时旧交换链还没被动过。
	void SwapChain::create(vk::raii::SwapchainKHR& out, vk::SwapchainKHR oldSwapchain)
	{
		const Support support = querySupport(m_context->physicalDevice, m_context->surface);

		const vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
		const vk::PresentModeKHR   presentMode = chooseSwapPresentMode(support.presentModes);
		const vk::Extent2D         extent = chooseSwapExtent(support.capabilities, m_window);

		uint32_t imageCount = support.capabilities.minImageCount + 1;
		if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
			imageCount = support.capabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR createInfo{};
		createInfo.surface = *m_context->surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

		QueueFamilyIndices indices = QueueFamilyIndices::findQueueFamilies(m_context->physicalDevice, m_context->surface);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = vk::SharingMode::eExclusive;//独占
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		createInfo.preTransform = support.capabilities.currentTransform;
		createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;//忽略a与其他窗口的混合
		createInfo.presentMode = presentMode;
		createInfo.clipped = vk::True;//启用裁剪
		createInfo.oldSwapchain = oldSwapchain;//调整窗口大小时传入旧交换链,实现原子重建

		out = vk::raii::SwapchainKHR(m_context->device, createInfo);

		m_images = out.getImages();
		m_imageFormat = surfaceFormat.format;
		m_extent = extent;
	}

	void SwapChain::recreate()
	{
		// 最小化时 framebuffer 尺寸为 0,空转等它恢复
		int width = 0, height = 0;
		glfwGetFramebufferSize(m_window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(m_window, &width, &height);
			glfwWaitEvents();
		}

		m_context->device.waitIdle();

		// 传旧交换链作为 oldSwapchain 原子重建;先用临时句柄创建,
		// 创建失败时旧交换链仍保持完整,下帧重试,避免 resize 期间抛异常导致窗口关闭。
		vk::SwapchainKHR old = *m_swapchain;

		vk::raii::SwapchainKHR fresh = nullptr;
		try {
			create(fresh, old);
		}
		catch (const std::exception& e) {
			std::cerr << "swap chain recreation deferred: " << e.what() << '\n';
			return;
		}

		// view 引用旧交换链的 image,必须先清 view 再换句柄
		m_imageViews.clear();
		m_swapchain = std::move(fresh);

		createImageViews();

		// 基数 = imageCount,新交换链的份数可能变了,所以是销毁重建不是复用。
		// 两个 create 函数都 assert 了"进去之前是空的",漏了这句会直接 abort。
		m_renderFinishedSemaphores.clear();
		createRenderFinishedSemaphores();
	}

	void SwapChain::createImageViews()
	{
		assert(m_imageViews.empty());

		m_imageViews.reserve(m_images.size());
		for (auto& image : m_images) {
			m_imageViews.emplace_back(Image::createView(image, m_imageFormat, m_context->device));
		}
	}

	void SwapChain::createRenderFinishedSemaphores()
	{
		assert(m_renderFinishedSemaphores.empty());

		// 基数 = imageCount,不是 MAX_FRAMES_IN_FLIGHT ——
		// present 等的信号量按 swapchain image 分,不是按帧槽分。
		// imageCount 会随重建变,所以要跟着一起重建。
		vk::SemaphoreCreateInfo semaphoreInfo{};

		m_renderFinishedSemaphores.reserve(m_images.size());
		for (size_t i = 0; i < m_images.size(); i++) {
			m_renderFinishedSemaphores.emplace_back(m_context->device, semaphoreInfo);
		}
	}
}
