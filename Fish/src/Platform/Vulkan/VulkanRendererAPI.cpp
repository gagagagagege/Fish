#include "VulkanRendererAPI.h"

#include "VulkanSurface.h"
#include "VulkanShader.h"
#include "VulkanVertexBuffer.h"
#include "VulkanTexture2D.h"

#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <string>
#include <vector>

namespace Fish {

	namespace {
		const std::vector<const char*> k_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
		const std::vector<const char*> k_DeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
		};
		constexpr uint32_t k_MaxFramesInFlight = 2;

		const float k_UnitQuad[4][2]     = { {-0.5f,-0.5f}, {0.5f,-0.5f}, {0.5f,0.5f}, {-0.5f,0.5f} };
		const float k_UnitQuadUv[4][2]   = { {0.0f,0.0f}, {1.0f,0.0f}, {1.0f,1.0f}, {0.0f,1.0f} };

		const uint32_t k_QuadOrder[6]    = { 0, 1, 2, 2, 3, 0 };

		const float k_UnitTriangle[3][2]   = { {-0.5f,-0.5f}, {0.5f,-0.5f}, {0.0f,0.5f} };
		const float k_UnitTriangleUv[3][2] = { {0.0f,0.0f}, {1.0f,0.0f}, {0.5f,1.0f} };

		constexpr uint32_t k_InitialTriangleVertices = 3000;
		constexpr uint32_t k_InitialQuadVertices     = 60000;

		void AppendShapeVertices(std::vector<BatchVertex>& out, const DrawBatch& batch)
		{
			auto emit = [&](const float local[2], const float uv[2]) {
				const glm::vec4 p = batch.transform * glm::vec4(local[0], local[1], 0.0f, 1.0f);
				out.push_back(BatchVertex{
					.position     = glm::vec3(p),
					.color        = batch.color,
					.uv           = glm::vec2(uv[0], uv[1]),
					.texId        = batch.texId,
					.tilingFactor = batch.tilingFactor });
			};

			if (batch.shape == BatchShape::Quad) {
				for (uint32_t i = 0; i < 6; ++i) {
					const uint32_t corner = k_QuadOrder[i];
					emit(k_UnitQuad[corner], k_UnitQuadUv[corner]);
				}
			}
			else {
				for (uint32_t i = 0; i < 3; ++i)
					emit(k_UnitTriangle[i], k_UnitTriangleUv[i]);
			}
		}

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

		// 批的顶点缓冲每帧槽一份,槽数跟着帧在飞数走。真正的 Buffer 推迟到
		// 第一次画那个批时才建 —— 那时才知道要多大。
		for (BatchState& batch : m_Batches) {
			batch.frames.resize(k_MaxFramesInFlight);
		}
	}

	void VulkanRendererAPI::RegisterBatchTexture(uint32_t slot, texture& tex)
	{
		if (slot >= k_MaxTextures) {
			throw std::runtime_error("纹理库槽位 " + std::to_string(slot) +
				" 越界,上限是 " + std::to_string(k_MaxTextures));
		}
		if (m_BatchTextureUsed[slot]) {
			if (m_BatchTextureInfos[slot].imageView == *tex.getView())
				return;
			throw std::runtime_error("纹理库槽位 " + std::to_string(slot) + " 已经被另一张贴图占了");
		}
		// 库是一次写完的,写完就不再动 —— 再登记进来的贴图不会出现在 set 里,
		// 而顶点里的 texId 照样指得到那个槽位,读到的是空描述符。
		if (m_BatchTextureSet != nullptr) {
			throw std::runtime_error("纹理库已经定下来了,槽位 " + std::to_string(slot) +
				" 登记得太晚 —— 所有贴图必须在第一次画批之前创建");
		}

		m_BatchTextureInfos[slot] = vk::DescriptorImageInfo{
			.sampler     = tex.getSampler(),
			.imageView   = tex.getView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };
		m_BatchTextureUsed[slot] = true;
	}

	void VulkanRendererAPI::SubmitBatch(const DrawBatch& batch)
	{
		BatchState& state = m_Batches[static_cast<uint32_t>(batch.shape)];

		if (state.shader == nullptr) {
			state.shader = batch.shader;
			state.viewProjection = batch.viewProjection;
		}
		else {
			FS_CORE_ASSERT(state.shader == batch.shader, "同一个批里两个 shader");
			FS_CORE_ASSERT(state.viewProjection == batch.viewProjection,
				"同一个批里的 viewProjection 不一致:一个批只能配一个相机");
		}

		AppendShapeVertices(state.vertices, batch);
	}

	void VulkanRendererAPI::EnsureBatchBuffer(BatchBuffer& target, uint32_t needed, uint32_t initial)
	{
		if (target.capacity >= needed)
			return;

		const uint32_t capacity = std::max(needed, std::max(initial, target.capacity * 2));

		Buffer buffer(static_cast<vk::DeviceSize>(capacity) * sizeof(BatchVertex),
			vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			m_DeviceContext.get());

		// 先 map 再搬。move 赋值会给旧的那份跑 unmap,mapped 必须在那之后才换。
		// 旧缓冲不用拷内容 —— 每帧都从 offset 0 重写,它那点数据是死的。
		void* mapped = buffer.map(0, buffer.getSize());
		target.buffer = std::move(buffer);
		target.mapped = mapped;
		target.capacity = capacity;
	}

	void VulkanRendererAPI::EnsureBatchTextureLibrary()
	{
		if (m_BatchTextureSet != nullptr)
			return;

		m_BatchTextureSet = m_DescriptorAllocator.allocateSet(k_BatchTextureSet);

		// 没登记的槽位也必须写一个合法的描述符进去。数组是按非一致索引取的,
		// 验证层证明不了"那几格没被访问",只认"每一格都写过" —— 留空洞就是
		// VUID-vkCmdDraw-None-08114,而且它拦不住,因为 descriptorBindingPartiallyBound
		// 只是允许"未写",不允许"未写还被取"。
		// 空洞填第一个登记过的贴图:正常的 texId 指不到那儿,填什么不影响画面。
		vk::DescriptorImageInfo filler{};
		for (uint32_t slot = 0; slot < k_MaxTextures; ++slot) {
			if (m_BatchTextureUsed[slot]) {
				filler = m_BatchTextureInfos[slot];
				break;
			}
		}
		if (filler.imageView == nullptr) {
			throw std::runtime_error("批的纹理库一个槽位都没登记,顶点里的 texId 没有东西可指");
		}

		std::vector<vk::DescriptorImageInfo> infos(k_MaxTextures, filler);
		for (uint32_t slot = 0; slot < k_MaxTextures; ++slot) {
			if (m_BatchTextureUsed[slot])
				infos[slot] = m_BatchTextureInfos[slot];
		}

		m_DescriptorAllocator.writeCombinedImageSamplers(m_BatchTextureSet,
			k_BatchTextureBinding, 0, infos);
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

	// 批处理段:每个批一次管线绑定 + 一次 pushConstants + 一次 draw,没有逐项循环。
	void VulkanRendererAPI::RecordBatches(vk::raii::CommandBuffer& commandBuffer)
	{
		if (!m_PipelineCache.has_value()) {
			// 一个 shader 都没建过(EnsureRenderState 提前返回了)。数据是这一帧的,
			// 清掉就行。
			for (BatchState& state : m_Batches)
				state.vertices.clear();
			return;
		}

		const uint32_t slot = m_Frames.currentIndex();

		for (uint32_t i = 0; i < k_BatchShapeCount; ++i) {
			BatchState& state = m_Batches[i];
			if (state.vertices.empty())
				continue;

			auto shader = std::dynamic_pointer_cast<VulkanShader>(state.shader);
			FS_CORE_ASSERT(shader, "批的 shader 不是 Vulkan 的");
			if (!shader)
				continue;

			const uint32_t vertexCount = static_cast<uint32_t>(state.vertices.size());
			const uint32_t initial = (i == static_cast<uint32_t>(BatchShape::Triangle))
				? k_InitialTriangleVertices : k_InitialQuadVertices;

			// 扩容点在这里是安全的:进 DrawFrame 时已经等过当前帧槽的 fence,
			// 这一槽上一轮的缓冲不再在飞,可以换掉。
			BatchBuffer& target = state.frames[slot];
			EnsureBatchBuffer(target, vertexCount, initial);

			memcpy(target.mapped, state.vertices.data(),
				static_cast<size_t>(vertexCount) * sizeof(BatchVertex));

			const PipelineDesc desc{
				.vertexModule   = shader->module(),
				.fragmentModule = shader->module(),
				.vertexEntry    = shader->vertexEntry(),
				.fragmentEntry  = shader->fragmentEntry(),
				.vertexStride   = k_BatchVertexStride
			};
			vk::raii::Pipeline& pipeline = m_PipelineCache->GetOrCreate(desc, shader->vertexInput());
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

			const PushConstants pushConstants{ .viewProj = state.viewProjection };
			commandBuffer.pushConstants<PushConstants>(m_PipelineCache->layout(),
				vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

			EnsureBatchTextureLibrary();
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				m_PipelineCache->layout(), k_BatchTextureSet, *m_BatchTextureSet, nullptr);

			commandBuffer.bindVertexBuffers(0, *target.buffer.getHandle(), { 0 });
			commandBuffer.draw(vertexCount, 1, 0, 0);
		}

		// 顶点已经进了 GPU 缓冲,CPU 侧这段这一帧不再需要。
		for (BatchState& state : m_Batches)
			state.vertices.clear();
	}

	//TODO: item循环里的dynamic_cast
	void VulkanRendererAPI::DrawFrame(const std::vector<DrawItem>& items)
	{
		EnsureRenderState();

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

		vk::Pipeline boundPipeline = nullptr;
		const void*  boundVertexBuffer = nullptr;
		const void*  boundIndexBuffer = nullptr;
		const void*  boundTexture = nullptr;

		// 环形缓冲的写游标归零。放在这儿而不是帧开头是安全的:到这个点已经
		// 等过这个帧槽的 fence,GPU 不再读它那块 UBO 了。
		frame.ResetObjects();

		for (const DrawItem& item : items) {
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

		// 批在 per-item 之后画。两条路并存,顺序是定死的:批里的东西盖在上面。
		RecordBatches(commandBuffer);

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
			m_FramebufferResized = true;
		}
		else if (result != vk::Result::eSuccess) {
			throw std::runtime_error("failed to present swap chain image");
		}

		m_Frames.advance();
	}

}
