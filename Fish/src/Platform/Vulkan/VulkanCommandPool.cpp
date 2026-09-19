#include "VulkanCommandPool.h"
#include "VulkanContext.h"

#include <stdexcept>

namespace Fish {
	CommandPool::CommandPool(VulkanContext* context)
		: m_context(context)
	{
		QueueFamilyIndices queueFamilyIndices = QueueFamilyIndices::findQueueFamilies(context->physicalDevice, context->surface);

		vk::CommandPoolCreateInfo poolInfo{};
		poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		m_pool = vk::raii::CommandPool(context->device, poolInfo);
	}

	vk::raii::CommandBuffer CommandPool::allocateBuffer() const
	{
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = *m_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };

		// 分配 1 个再 move 出来。vk::raii::CommandBuffers 只是 std::vector 的子类、
		// 没有析构函数,被搬走的那个元素在 vector 里变成空句柄,临时对象析构时跳过它。
		return std::move(vk::raii::CommandBuffers(m_context->device, allocInfo).front());
	}

	void recordCommandBuffer(vk::raii::CommandBuffer& commandBuffer,
		SwapChain& swapChain, uint32_t imageIndex, Pipeline& pipeline,
		const Buffer& vertexBuffer, const std::vector<Vertex>& vertices,
		const Buffer& indexBuffer, const std::vector<uint16_t>& indices,
		vk::raii::DescriptorSet& descriptorSet)
	{
		const vk::Image    targetImage = swapChain.images()[imageIndex];
		const vk::Extent2D swapChainExtent = swapChain.extent();

		vk::CommandBufferBeginInfo beginInfo{};

		commandBuffer.begin(beginInfo);

		// 交换链图像布局转换: Undefined -> ColorAttachmentOptimal(render pass 曾自动处理)
		vk::ImageMemoryBarrier colorBarrier{};
		colorBarrier.oldLayout = vk::ImageLayout::eUndefined;
		colorBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorBarrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
		colorBarrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
		colorBarrier.image = targetImage;
		colorBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		colorBarrier.subresourceRange.baseMipLevel = 0;
		colorBarrier.subresourceRange.levelCount = 1;
		colorBarrier.subresourceRange.baseArrayLayer = 0;
		colorBarrier.subresourceRange.layerCount = 1;
		colorBarrier.srcAccessMask = vk::AccessFlagBits::eNone;
		colorBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::DependencyFlags{}, nullptr, nullptr, colorBarrier);

		//动态渲染:替代 render pass 的 beginRenderPass
		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

		vk::RenderingAttachmentInfo colorAttachment{};
		colorAttachment.imageView = *swapChain.imageView(imageIndex);
		colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.clearValue = clearColor;

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
		renderingInfo.renderArea.extent = swapChainExtent;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		commandBuffer.beginRendering(renderingInfo);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.handle());

		//动态定义
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		commandBuffer.setViewport(0, viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = swapChainExtent;
		commandBuffer.setScissor(0, scissor);

		commandBuffer.bindVertexBuffers(0, *vertexBuffer.getHandle(), vk::DeviceSize{ 0 });
		commandBuffer.bindIndexBuffer(*indexBuffer.getHandle(), 0, vk::IndexType::eUint16);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.layout(), 0, *descriptorSet, nullptr);

		commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		commandBuffer.endRendering();

		// 交换链图像布局转换: ColorAttachmentOptimal -> PresentSrcKHR
		vk::ImageMemoryBarrier presentBarrier{};
		presentBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
		presentBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
		presentBarrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
		presentBarrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
		presentBarrier.image = targetImage;
		presentBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		presentBarrier.subresourceRange.baseMipLevel = 0;
		presentBarrier.subresourceRange.levelCount = 1;
		presentBarrier.subresourceRange.baseArrayLayer = 0;
		presentBarrier.subresourceRange.layerCount = 1;
		presentBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		presentBarrier.dstAccessMask = vk::AccessFlagBits::eNone;

		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags{}, nullptr, nullptr, presentBarrier);

		commandBuffer.end();
	}
	vk::raii::CommandBuffer beginSingleTimeCommands(VulkanContext* context, CommandPool& transientPool)
	{
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = transientPool.handle(), .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
		vk::raii::CommandBuffer       commandBuffer = std::move(vk::raii::CommandBuffers(context->device, allocInfo).front());

		vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		commandBuffer.begin(beginInfo);

		return std::move(commandBuffer);
	}
	void endSingleTimeCommands(VulkanContext* context, vk::raii::CommandBuffer& commandBuffer)
	{
		commandBuffer.end();

		vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
		context->graphicsQueue.submit(submitInfo, nullptr);
		context->graphicsQueue.waitIdle();
	}
}
