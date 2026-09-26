#include "VulkanVertexBuffer.h"

#include "VulkanContext.h"
#include "VulkanCommandPool.h"

namespace Fish {
	VulkanVertexBuffer::VulkanVertexBuffer(VulkanContext* context, CommandPool& transientPool,
		const void* data, size_t size, uint32_t stride)
		: m_stride(stride)
	{
		m_count = stride ? static_cast<uint32_t>(size / stride) : 0;

		m_buffer = Buffer::createDeviceLocal(data, size, vk::BufferUsageFlagBits::eVertexBuffer,
			context, transientPool);
	}

	VulkanIndexBuffer::VulkanIndexBuffer(VulkanContext* context, CommandPool& transientPool,
		const uint32_t* indices, uint32_t count)
		: m_count(count)
	{
		m_buffer = Buffer::createDeviceLocal(indices, sizeof(uint32_t) * count,
			vk::BufferUsageFlagBits::eIndexBuffer, context, transientPool);
	}
}
