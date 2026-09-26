#include "VulkanFrameData.h"

#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "VulkanDescriptorAllocator.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace Fish {
	namespace {
		uint32_t AlignUp(uint32_t value, uint32_t alignment)
		{
			return (value + alignment - 1) / alignment * alignment;
		}
	}

	FrameData::FrameData(VulkanContext* context, CommandPool& commandPool)
		: m_context(context)
	{
		// dynamic offset 必须是这个值的倍数,否则验证层报
		// VUID-vkCmdBindDescriptorSets-pDynamicOffsets-01779
		const uint32_t alignment = static_cast<uint32_t>(
			context->physicalDevice.getProperties().limits.minUniformBufferOffsetAlignment);
		m_uboStride = AlignUp(static_cast<uint32_t>(sizeof(ObjectUbo)), alignment);

		CreateUbo(k_InitialObjectsPerFrame);

		m_commandBuffer = commandPool.allocateBuffer();

		m_presentCompleteSemaphore = vk::raii::Semaphore(context->device, vk::SemaphoreCreateInfo{});

		// fence 建出来必须已经是 signaled,否则第一帧死等
		vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };
		m_inFlightFence = vk::raii::Fence(context->device, fenceInfo);
	}

	void FrameData::CreateUbo(uint32_t capacity)
	{
		m_uniformBuffer = Buffer(static_cast<vk::DeviceSize>(m_uboStride) * capacity,
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			m_context);

		m_uboMappedBase = m_uniformBuffer.map(0, m_uniformBuffer.getSize());
		m_uboCapacity = capacity;
	}

	void FrameData::EnsureDescriptorSet(DescriptorAllocator& descriptorAllocator)
	{
		if (*m_descriptorSet != nullptr)
			return;

		m_descriptorSet = descriptorAllocator.allocateSet(k_ObjectUboSet);

		descriptorAllocator.writeUniformBuffer(m_descriptorSet, k_ObjectUboBinding,
			vk::DescriptorBufferInfo{ .buffer = m_uniformBuffer.getHandle(), .offset = 0,
				.range = sizeof(ObjectUbo) });
	}

	void FrameData::EnsureUboCapacity(DescriptorAllocator& descriptorAllocator, uint32_t objectCount)
	{
		if (objectCount <= m_uboCapacity)
			return;

		CreateUbo(std::max(objectCount, m_uboCapacity * 2));

		if (*m_descriptorSet != nullptr) {
			descriptorAllocator.writeUniformBuffer(m_descriptorSet, k_ObjectUboBinding,
				vk::DescriptorBufferInfo{ .buffer = m_uniformBuffer.getHandle(), .offset = 0,
					.range = sizeof(ObjectUbo) });
		}
	}

	uint32_t FrameData::PushObjectUbo(const glm::mat4& model, const glm::vec4& color)
	{
		const ObjectUbo data{ .model = model, .color = color };
		memcpy(static_cast<char*>(m_uboMappedBase) + m_objectCount * m_uboStride,
			&data, sizeof(data));

		return m_objectCount++ * m_uboStride;
	}

	Frames::Frames(uint32_t frameCount, VulkanContext* context, CommandPool& commandPool)
	{
		// reserve 掉,避免扩容时移动已有元素
		m_frames.reserve(frameCount);
		for (uint32_t i = 0; i < frameCount; i++) {
			m_frames.emplace_back(context, commandPool);
		}
	}

	void Frames::EnsureDescriptorSets(DescriptorAllocator& descriptorAllocator)
	{
		for (FrameData& frame : m_frames) {
			frame.EnsureDescriptorSet(descriptorAllocator);
		}
	}

	void Frames::EnsureUboCapacities(DescriptorAllocator& descriptorAllocator, uint32_t objectCount)
	{
		for (FrameData& frame : m_frames) {
			frame.EnsureUboCapacity(descriptorAllocator, objectCount);
		}
	}
}
