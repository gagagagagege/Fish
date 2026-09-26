#include "VulkanRendererAPI.h"

#include "VulkanSurface.h"
#include "VulkanShader.h"
#include "VulkanVertexBuffer.h"
#include "VulkanTexture2D.h"

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

		m_CommandPool   = CommandPool(m_DeviceContext.get());
		m_TransientPool = CommandPool(m_DeviceContext.get());

		m_Frames = Frames(k_MaxFramesInFlight, m_DeviceContext.get(), m_CommandPool);
	}

	// 管线缓存、贴图 set 缓存和每个帧槽的描述符集都推迟到这里 —— 它们都依赖
	// 描述符 layout,而那个要等应用层创建第一个 shader。一个 shader 都没有就
	// 直接返回:这一帧本来也没东西可画。
	void VulkanRendererAPI::EnsureRenderState()
	{
		if (!m_DescriptorAllocator.hasLayout())
			return;

		m_Frames.EnsureDescriptorSets(m_DescriptorAllocator);

		const vk::Format swapChainFormat = m_SwapChain.imageFormat();

		if (m_PipelineCache.has_value()) {
			if (m_PipelineCache->swapChainFormat() != swapChainFormat)
				m_PipelineCache->Invalidate(swapChainFormat);
			return;
		}

		m_TextureSets.emplace(m_DeviceContext.get(), m_DescriptorAllocator);
		m_PipelineCache.emplace(m_DeviceContext.get(),
			m_DescriptorAllocator.layoutHandles(),
			std::vector<vk::PushConstantRange>{ vk::PushConstantRange{
				.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
				.offset = 0,
				.size = sizeof(PushConstants) } },
			swapChainFormat);
	}

	VulkanRendererAPI::~VulkanRendererAPI()
	{
		// 必须等 GPU 空转再析构
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

	void VulkanRendererAPI::WaitIdle()
	{
		if (m_DeviceContext) {
			m_DeviceContext->device.waitIdle();
		}
	}

	void VulkanRendererAPI::DrawFrame(const std::vector<DrawItem>& items)
	{
		EnsureRenderState();

		// 重建放在 acquire 之前 —— 之后重建会把刚 acquire 到的图像丢掉。
		if (m_FramebufferResized) {
			m_FramebufferResized = false;
			m_SwapChain.recreate();
		}

		FrameData& frame = m_Frames.current();

		if (m_DeviceContext->device.waitForFences(*frame.inFlightFence(), vk::True, UINT64_MAX) != vk::Result::eSuccess) {
			throw std::runtime_error("failed to wait for in-flight fence");
		}

		frame.EnsureUboCapacity(m_DescriptorAllocator, static_cast<uint32_t>(items.size()));

		auto [result, imageIndex] = m_SwapChain.handle().acquireNextImage(
			UINT64_MAX, *frame.presentCompleteSemaphore(), nullptr);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eErrorSurfaceLostKHR) {
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

		// 清屏在这里
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

		// 各 layer 攒下的绘制项在这里录成命令。
		//
		// 管线 / 顶点缓冲 / 索引缓冲 / 贴图都是"变了才发" —— 绑定在命令缓冲里
		// 是持久的,重复发同一个东西只是白占命令缓冲,执行时驱动还要重走一遍。
		// set 0 例外,每项都发:它的 dynamic offset 每个物体都不同,去重不了。
		vk::Pipeline boundPipeline = nullptr;
		const void*  boundVertexBuffer = nullptr;
		const void*  boundIndexBuffer = nullptr;
		const void*  boundTexture = nullptr;

		// 环形缓冲的写游标归零。放在这儿而不是帧开头是安全的:到这个点已经
		// 等过这个帧槽的 fence,GPU 不再读它那块 UBO 了。
		frame.ResetObjects();

		for (const DrawItem& item : items) {
			// Submit 收的是 Fish 的抽象类型,这里降到后端实现。
			// 转不出来说明那个绘制项的 shader 不是 Vulkan 的 —— 跳过,
			// 而不是拿空指针往下走。
			auto shader = std::dynamic_pointer_cast<VulkanShader>(item.shader);
			FS_CORE_ASSERT(shader, "DrawItem.shader 不是 Vulkan 的");
			if (!shader)
				continue;

			auto vertexBuffer = std::dynamic_pointer_cast<VulkanVertexBuffer>(item.vertexBuffer);
			FS_CORE_ASSERT(vertexBuffer, "DrawItem.vertexBuffer 不是 Vulkan 的");
			if (!vertexBuffer)
				continue;

			auto indexBuffer = std::dynamic_pointer_cast<VulkanIndexBuffer>(item.indexBuffer);
			if (item.indexBuffer) {
				FS_CORE_ASSERT(indexBuffer, "DrawItem.indexBuffer 不是 Vulkan 的");
				if (!indexBuffer)
					continue;
			}

			PipelineDesc desc{
				.vertexModule   = shader->module(),
				.fragmentModule = shader->module(),
				.vertexEntry    = shader->vertexEntry(),
				.fragmentEntry  = shader->fragmentEntry(),
				.vertexStride   = vertexBuffer->stride()
			};
			vk::raii::Pipeline& pipeline = m_PipelineCache->GetOrCreate(desc, shader->vertexInput());
			if (*pipeline != boundPipeline) {
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
				boundPipeline = *pipeline;
			}

			const PushConstants pushConstants{ .viewProj = item.viewProjection };
			commandBuffer.pushConstants<PushConstants>(m_PipelineCache->layout(),
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

			const uint32_t dynamicOffset = frame.PushObjectUbo(item.transform, item.color);
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				m_PipelineCache->layout(), k_ObjectUboSet, *frame.descriptorSet(), dynamicOffset);

			// 没有贴图的绘制项不绑 set 1 —— 它的管线不采样。
			// boundTexture 不复位:描述符集绑定是持久的,set 1 还绑着上一次那张,
			// 换回来的时候不用重绑。
			if (item.texture) {
				auto texture = std::dynamic_pointer_cast<VulkanTexture2D>(item.texture);
				FS_CORE_ASSERT(texture, "DrawItem.texture 不是 Vulkan 的");
				if (texture && texture.get() != boundTexture) {
					vk::raii::DescriptorSet& textureSet = m_TextureSets->GetOrCreate(
						texture->getTexture().getView(), texture->getTexture().getSampler());
					commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
						m_PipelineCache->layout(), k_TextureSet, *textureSet, nullptr);
					boundTexture = texture.get();
				}
			}

			if (vertexBuffer.get() != boundVertexBuffer) {
				commandBuffer.bindVertexBuffers(0, *vertexBuffer->buffer().getHandle(), { 0 });
				boundVertexBuffer = vertexBuffer.get();
			}

			if (indexBuffer) {
				if (indexBuffer.get() != boundIndexBuffer) {
					commandBuffer.bindIndexBuffer(*indexBuffer->buffer().getHandle(), 0,
						vk::IndexType::eUint32);
					boundIndexBuffer = indexBuffer.get();
				}
				commandBuffer.drawIndexed(indexBuffer->GetCount(), 1, 0, 0, 0);
			}
			else {
				commandBuffer.draw(vertexBuffer->count(), 1, 0, 0);
			}
		}

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

}
