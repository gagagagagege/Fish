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
