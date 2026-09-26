#include "VulkanBuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanContext.h"

#include <array>
#include <cstddef>
#include <cstring>

namespace Fish {
	Buffer::Buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags& properties, VulkanContext* context):m_size(size),m_context(context)
	{
		vk::BufferCreateInfo bufferInfo{};
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;

		m_buffer = vk::raii::Buffer(m_context->device, bufferInfo);

		vk::MemoryRequirements memRequirements = m_buffer.getMemoryRequirements();

		vk::MemoryAllocateInfo allocInfo{};
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties, m_context->physicalDevice);

		m_memory = vk::raii::DeviceMemory(m_context->device, allocInfo);
		m_buffer.bindMemory(m_memory, 0);
	}

	void Buffer::copyBuffer(const vk::raii::Buffer& srcBuffer, CommandPool& transientPool)
	{
		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands(m_context, transientPool);

		commandBuffer.copyBuffer(srcBuffer, m_buffer, vk::BufferCopy{ .size = m_size });

		endSingleTimeCommands(m_context, commandBuffer);
		// 临时命令缓冲随 transientPool 销毁时一并释放
	}

	Buffer Buffer::createDeviceLocal(const void* data, vk::DeviceSize size,
		vk::BufferUsageFlags usage, VulkanContext* context, CommandPool& transientPool)
	{
		Buffer stagingBuffer(size, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, context);

		memcpy(stagingBuffer.map(0, size), data, static_cast<size_t>(size));
		stagingBuffer.unmap();

		Buffer dstBuffer(size, vk::BufferUsageFlagBits::eTransferDst | usage,
			vk::MemoryPropertyFlagBits::eDeviceLocal, context);
		dstBuffer.copyBuffer(stagingBuffer.getHandle(), transientPool);

		return dstBuffer;
	}

	uint32_t Buffer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, vk::raii::PhysicalDevice& physicalDevice)
	{
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}
	void* Buffer::map(vk::DeviceSize offset, vk::DeviceSize size)
	{
		if (m_mapPtr) return m_mapPtr;
		m_mapPtr = m_memory.mapMemory(offset, size);
		return m_mapPtr;
	}
	void Buffer::unmap()
	{
		if (!m_mapPtr) return;
		m_memory.unmapMemory();
		m_mapPtr = nullptr;
	}
}
