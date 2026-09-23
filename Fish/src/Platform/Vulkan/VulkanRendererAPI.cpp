#include "VulkanRendererAPI.h"

#include "VulkanSurface.h"

#include <stdexcept>
#include <cstring>

namespace Fish {

	namespace {
		const std::vector<const char*> k_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
		const std::vector<const char*> k_DeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
		};
		constexpr uint32_t k_MaxFramesInFlight = 2;

		void transitionSwapImage(vk::raii::CommandBuffer& commandBuffer, vk::Image image,
			vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
			vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
			vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage)
		{
			vk::ImageMemoryBarrier barrier{};
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
			barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = srcAccess;
			barrier.dstAccessMask = dstAccess;

			commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags{}, nullptr, nullptr, barrier);
		}
	}

	void VulkanRendererAPI::Init(void* nativeWindow)
	{
		m_Window = static_cast<GLFWwindow*>(nativeWindow);
		if (m_Window == nullptr) {
			throw std::runtime_error("VulkanRendererAPI::Init: nativeWindow is null");
		}

		createInstance();
		setupDebugMessenger(m_Instance, m_DebugMessenger);
		surface::createWin32Surface(m_Instance, m_Surface, m_Window);

		m_DeviceContext = std::make_unique<VulkanContext>(m_Instance, m_Surface, k_DeviceExtensions, k_ValidationLayers);

		m_SwapChain = SwapChain(m_DeviceContext.get(), m_Window);

		m_DescriptorAllocator = DescriptorAllocator(m_DeviceContext.get(), k_MaxFramesInFlight);

		const std::vector<vk::DynamicState> dynamicStates = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		m_Pipeline = Pipeline(m_DeviceContext.get(), dynamicStates, m_SwapChain.imageFormat(), m_DescriptorAllocator.layout());

		m_CommandPool   = CommandPool(m_DeviceContext.get());
		m_TransientPool = CommandPool(m_DeviceContext.get());

		m_MainTexture = texture::loadFromFile(m_DeviceContext.get(), m_TransientPool,
			"Fish/src/Platform/Vulkan/textures/texture.jpg");

		m_Frames = Frames(k_MaxFramesInFlight, m_DeviceContext.get(), m_CommandPool, m_DescriptorAllocator,
			m_MainTexture.getView(), m_MainTexture.getSampler());
	}

	VulkanRendererAPI::~VulkanRendererAPI()
	{
		// 必须等 GPU 空转再析构 —— 最后一帧的 submit / present 可能还在飞,
		// 不等就会去销毁正被 queue 使用的 fence / 信号量 / 命令缓冲 / 交换链。
		// (原型 TriangleApp::cleanUp 里就是这么写的,搬过来时漏了)
		// 函数体先于成员析构执行,所以这里所有 vk::raii 成员还活着。
		if (m_DeviceContext) {
			m_DeviceContext->device.waitIdle();
		}
	}

	void VulkanRendererAPI::createInstance()
	{
		if (enableValidationLayers && !checkValidationLayerSupport(m_Context, k_ValidationLayers)) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		vk::ApplicationInfo appInfo{};
		appInfo.pApplicationName = "Fish";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Fish";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		auto requiredExtensions = getRequiredExtensions();

		// vkCreateInstance 自己会因为缺扩展失败,但只给 VK_ERROR_EXTENSION_NOT_PRESENT,
		// 看不出缺的是哪个。先查一遍把名字带进异常。
		auto availableExtensions = m_Context.enumerateInstanceExtensionProperties();
		for (const char* required : requiredExtensions) {
			const bool found = std::ranges::any_of(availableExtensions, [required](const auto& e) {
				return strcmp(required, e.extensionName) == 0;
			});
			if (!found) {
				throw std::runtime_error(std::string("required instance extension not supported: ") + required);
			}
		}

		vk::InstanceCreateInfo createInfo{};
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(k_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = k_ValidationLayers.data();
		}

		m_Instance = vk::raii::Instance(m_Context, createInfo);
	}

	void VulkanRendererAPI::NotifyWindowResized()
	{
		m_FramebufferResized = true;
	}

	void VulkanRendererAPI::SetClearColor(const glm::vec4& color)
	{
		m_ClearColor = color;
	}

	void VulkanRendererAPI::DrawFrame()
	{
		// 重建放在 acquire 之前 —— 之后重建会把刚 acquire 到的图像丢掉。
		if (m_FramebufferResized) {
			m_FramebufferResized = false;
			m_SwapChain.recreate();
		}

		FrameData& frame = m_Frames.current();

		if (m_DeviceContext->device.waitForFences(*frame.inFlightFence(), vk::True, UINT64_MAX) != vk::Result::eSuccess) {
			throw std::runtime_error("failed to wait for in-flight fence");
		}

		auto [result, imageIndex] = m_SwapChain.handle().acquireNextImage(
			UINT64_MAX, *frame.presentCompleteSemaphore(), nullptr);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eErrorSurfaceLostKHR) {
			// 这一帧没图可画,整个丢掉。acquire 和 present 在同一个函数里,
			// 直接 return 不会破坏什么配对关系 —— 拆成 Begin/End 时才会。
			m_SwapChain.recreate();
			return;
		}
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
			throw std::runtime_error("failed to acquire swap chain image");
		}

		m_DeviceContext->device.resetFences(*frame.inFlightFence());

		vk::raii::CommandBuffer& commandBuffer = frame.commandBuffer();
		commandBuffer.reset();
		commandBuffer.begin(vk::CommandBufferBeginInfo{});

		// oldLayout 用 Undefined,含义是"上一帧画了什么不管",所以不用记住它原来的布局。
		transitionSwapImage(commandBuffer, m_SwapChain.images()[imageIndex],
			vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput);

		// 清屏在这里发生:动态渲染没有 render pass,载入行为靠 loadOp + clearValue。
		vk::RenderingAttachmentInfo colorAttachment{};
		colorAttachment.imageView = *m_SwapChain.imageView(imageIndex);
		colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.clearValue = vk::ClearColorValue(m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a);

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea = vk::Rect2D{ vk::Offset2D{ 0, 0 }, m_SwapChain.extent() };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		commandBuffer.beginRendering(renderingInfo);

		const vk::Extent2D extent = m_SwapChain.extent();

		vk::Viewport viewport{};
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		commandBuffer.setViewport(0, viewport);

		vk::Rect2D scissor{};
		scissor.extent = extent;
		commandBuffer.setScissor(0, scissor);

		// 这里以后是各 layer 攒下的 draw 命令 —— Submit 只入队,不碰命令缓冲。

		commandBuffer.endRendering();

		transitionSwapImage(commandBuffer, m_SwapChain.images()[imageIndex],
			vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
			vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe);

		commandBuffer.end();

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo submitInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*frame.presentCompleteSemaphore(),
			.pWaitDstStageMask = &waitDestinationStageMask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*m_SwapChain.renderFinishedSemaphore(imageIndex)
		};
		m_DeviceContext->graphicsQueue.submit(submitInfo, *frame.inFlightFence());

		const vk::PresentInfoKHR presentInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*m_SwapChain.renderFinishedSemaphore(imageIndex),
			.swapchainCount = 1,
			.pSwapchains = &*m_SwapChain.handle(),
			.pImageIndices = &imageIndex
		};
		result = m_DeviceContext->presentQueue.presentKHR(presentInfo);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR
			|| result == vk::Result::eErrorSurfaceLostKHR) {
			// 下帧开头重建,不在这里做。
			m_FramebufferResized = true;
		}
		else if (result != vk::Result::eSuccess) {
			throw std::runtime_error("failed to present swap chain image");
		}

		m_Frames.advance();
	}

	void VulkanRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray)
	{
		throw std::runtime_error("VulkanRendererAPI::DrawIndexed not implemented yet (needs the W6 pipeline)");
	}
}
