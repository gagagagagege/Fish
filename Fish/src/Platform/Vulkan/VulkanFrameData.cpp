#include "VulkanFrameData.h"

#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "VulkanDescriptorAllocator.h"

namespace Fish {
	FrameData::FrameData(VulkanContext* context,
		CommandPool& commandPool,
		DescriptorAllocator& descriptorAllocator,
		vk::raii::ImageView& textureView,
		vk::raii::Sampler& textureSampler)
	{
		// UBO:CPU 每帧要写,所以是 host visible + coherent
		m_uniformBuffer = Buffer(sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, context);

		m_commandBuffer = commandPool.allocateBuffer();

		// 描述符集当场绑上自己那份 UBO —— 配对在构造函数里完成,外面没有搞错的机会
		m_descriptorSet = descriptorAllocator.allocate(m_uniformBuffer, textureView, textureSampler);

		m_presentCompleteSemaphore = vk::raii::Semaphore(context->device, vk::SemaphoreCreateInfo{});

		// fence 建出来必须已经是 signaled,否则第一帧死等
		vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };
		m_inFlightFence = vk::raii::Fence(context->device, fenceInfo);
	}

	Frames::Frames(uint32_t frameCount, VulkanContext* context,
		CommandPool& commandPool,
		DescriptorAllocator& descriptorAllocator,
		vk::raii::ImageView& textureView,
		vk::raii::Sampler& textureSampler)
	{
		// reserve 掉,避免扩容时移动已有元素
		m_frames.reserve(frameCount);
		for (uint32_t i = 0; i < frameCount; i++) {
			m_frames.emplace_back(context, commandPool, descriptorAllocator, textureView, textureSampler);
		}
	}
}
