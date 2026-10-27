#include "VulkanRendererAPI.h"

#include "VulkanSurface.h"
#include "VulkanShader.h"
#include "VulkanVertexBuffer.h"
#include "VulkanTexture2D.h"

#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <string>
#include <utility>
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
					.samplerId    = batch.samplerId,
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

	// 发号优先用回收回来的,没有才往上加。两个表各管各的。
	uint32_t VulkanRendererAPI::TakeImageSlot()
	{
		if (!m_FreeImageSlots.empty()) {
			const uint32_t slot = m_FreeImageSlots.back();
			m_FreeImageSlots.pop_back();
			return slot;
		}
		if (m_NextImageSlot >= m_ImageCapacity) {
			throw std::runtime_error("纹理表满了:已经发了 " + std::to_string(m_NextImageSlot) +
				" 个槽位,上限是 " + std::to_string(m_ImageCapacity));
		}
		return m_NextImageSlot++;
	}

	uint32_t VulkanRendererAPI::TakeSamplerSlot()
	{
		if (!m_FreeSamplerSlots.empty()) {
			const uint32_t slot = m_FreeSamplerSlots.back();
			m_FreeSamplerSlots.pop_back();
			return slot;
		}
		if (m_NextSamplerSlot >= m_SamplerCapacity) {
			throw std::runtime_error("采样器表满了:已经发了 " + std::to_string(m_NextSamplerSlot) +
				" 个槽位,上限是 " + std::to_string(m_SamplerCapacity));
		}
		return m_NextSamplerSlot++;
	}

	VulkanRendererAPI::BindlessTextureIndices VulkanRendererAPI::RegisterTexture(texture& tex)
	{
		// 先问表多大 —— 表的格数就是 layout 的 descriptorCount,所以没登记过
		// shader 时这里会抛,不会发出去一个指不到任何东西的槽位。
		m_ImageCapacity   = m_DescriptorAllocator.sampledImageTableSize();
		m_SamplerCapacity = m_DescriptorAllocator.samplerTableSize();

		// 两个号一起拿。后一个抛时前一个会白丢 —— 那条路是终止进程的异常路径,
		// 不值得为它写回滚。
		const uint32_t imageSlot   = TakeImageSlot();
		const uint32_t samplerSlot = TakeSamplerSlot();

		const vk::DescriptorImageInfo imageInfo{
			.imageView   = tex.getView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };
		const vk::DescriptorImageInfo samplerInfo{ .sampler = tex.getSampler() };

		if (m_ImageInfos.size() <= imageSlot)
			m_ImageInfos.resize(imageSlot + 1);
		m_ImageInfos[imageSlot] = imageInfo;

		if (m_SamplerInfos.size() <= samplerSlot)
			m_SamplerInfos.resize(samplerSlot + 1);
		m_SamplerInfos[samplerSlot] = samplerInfo;

		// 表已经建出来了,那就不用等 —— 只写这两格,别的格不碰。
		// 这一步合法靠的是 set 上那两个 flag(见 DescriptorAllocator::BuildLayout)。
		if (m_TextureTableSet != nullptr) {
			m_DescriptorAllocator.writeSampledImages(m_TextureTableSet,
				k_TextureImageBinding, imageSlot, { imageInfo });
			m_DescriptorAllocator.writeSamplers(m_TextureTableSet,
				k_TextureSamplerBinding, samplerSlot, { samplerInfo });
		}

		return { imageSlot, samplerSlot };
	}

	void VulkanRendererAPI::ReleaseTexture(texture&& gpu, uint32_t imageSlot, uint32_t samplerSlot)
	{
		// 构造之后、注册之前就析构:没有任何命令引用过它,当场销毁。
		if (imageSlot == k_InvalidBindlessIndex && samplerSlot == k_InvalidBindlessIndex)
			return;

		m_DeferredTextures.push_back(DeferredTexture{
			.gpu         = std::move(gpu),
			.imageSlot   = imageSlot,
			.samplerSlot = samplerSlot,
			.frame       = m_FrameNumber });
	}

	void VulkanRendererAPI::ReclaimTextures()
	{
		// 帧 #j 提交的工作用第 j 帧槽;同一槽的下一次 fence 等待在帧 #j+2 的开头。
		// 本函数只在那个等待之后调,所以"frame + 帧在飞数 <= 当前帧号"就够。
		while (!m_DeferredTextures.empty()) {
			DeferredTexture& oldest = m_DeferredTextures.front();
			if (oldest.frame + k_MaxFramesInFlight > m_FrameNumber)
				break;

			if (oldest.imageSlot != k_InvalidBindlessIndex)
				m_FreeImageSlots.push_back(oldest.imageSlot);
			if (oldest.samplerSlot != k_InvalidBindlessIndex)
				m_FreeSamplerSlots.push_back(oldest.samplerSlot);

			// pop_front 析构 texture —— image/view/sampler 到这一刻才真正销毁。
			m_DeferredTextures.pop_front();
		}
	}

	void VulkanRendererAPI::SubmitBatch(const DrawBatch& batch)
	{
		BatchState& state = m_Batches[static_cast<uint32_t>(batch.shape)];

		if (state.shader == nullptr) {
			state.shader = batch.shader;
		}
		else {
			FS_CORE_ASSERT(state.shader == batch.shader, "同一个批里两个 shader");
		}

		// 每次提交都重记。viewProjection 是"这一帧用哪个相机",不是"这个批永远用
		// 哪个相机" —— 只在第一次赋值会让整个批冻在第一帧的相机上,而 resize 和
		// 最小化都会重算相机的投影矩阵,之后批里的顶点还是按老矩阵变换。
		state.viewProjection = batch.viewProjection;

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

	// 表是懒建的:格数要在 layout 出来之后才知道(那是第一个 shader 的事)。
	// 建的那一刻把之前攒下的槽一次写进去,之后就只走 RegisterTexture 那条增量路。
	void VulkanRendererAPI::EnsureTextureTable()
	{
		if (m_TextureTableSet != nullptr)
			return;

		m_TextureTableSet = m_DescriptorAllocator.allocateSet(k_TextureSet);

		// 没写过的格不填:binding 上带着 ePartiallyBound,表里允许有空洞。
		m_DescriptorAllocator.writeSampledImages(m_TextureTableSet,
			k_TextureImageBinding, 0, m_ImageInfos);
		m_DescriptorAllocator.writeSamplers(m_TextureTableSet,
			k_TextureSamplerBinding, 0, m_SamplerInfos);
	}

	// 管线缓存、贴图 set 缓存和每个帧槽的描述符集都推迟到这里 —— 它们都依赖
	// 描述符 layout,而那个要等应用层创建第一个 shader。一个 shader 都没有就
	// 直接返回:这一帧本来也没东西可画。
	void VulkanRendererAPI::EnsureRenderState()
	{
		if (!m_DescriptorAllocator.hasLayout())
			return;

		m_Frames.EnsureDescriptorSets(m_DescriptorAllocator);
		EnsureTextureTable();

		const vk::Format swapChainFormat = m_SwapChain.imageFormat();

		if (m_PipelineCache.has_value()) {
			if (m_PipelineCache->swapChainFormat() != swapChainFormat)
				m_PipelineCache->Invalidate(swapChainFormat);
			return;
		}

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

		// 帧号在等过 fence 之后才自增 —— 这个等待才是回收判据的来源。
		++m_FrameNumber;
		ReclaimTextures();

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

		// 纹理表只绑一次,两条路共用 —— 纹理选择已经是 UBO 里的一个数,
		// 不再是"换一张图就重绑 set 1"。所以它也就没有 per-item 的重绑可省了。
		if (m_TextureTableSet != nullptr) {
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				m_PipelineCache->layout(), k_TextureSet, *m_TextureTableSet, nullptr);
		}

		vk::Pipeline boundPipeline = nullptr;
		const void*  boundVertexBuffer = nullptr;
		const void*  boundIndexBuffer = nullptr;

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

			// 两个槽位都跟着 UBO 走 —— 顶点格式(stride 20)塞不下更多属性。
			// 没有贴图的项给 0:它的管线不采样,指到哪格都一样。
			const uint32_t texId     = item.texture ? item.texture->GetBindlessIndex() : 0u;
			const uint32_t samplerId = item.texture ? item.texture->GetSamplerIndex()  : 0u;
			const uint32_t dynamicOffset = frame.PushObjectUbo(item.transform, item.color, texId, samplerId);
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				m_PipelineCache->layout(), k_ObjectUboSet, *frame.descriptorSet(), dynamicOffset);

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
