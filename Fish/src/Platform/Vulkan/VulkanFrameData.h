#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "VulkanBuffer.h"

#include "glm/glm.hpp"

#include <cstdint>
#include <vector>

namespace Fish {
	class VulkanContext;
	class CommandPool;
	class DescriptorAllocator;

	struct ObjectUbo
	{
		glm::mat4 model;
		glm::vec4 color;
	};

	struct PushConstants
	{
		glm::mat4 viewProj;
	};

	// 环形缓冲的起始容量
	inline constexpr uint32_t k_InitialObjectsPerFrame = 128;

	class FrameData
	{
	public:
		FrameData() = default;
		FrameData(VulkanContext* context, CommandPool& commandPool);
		~FrameData() = default;

		vk::raii::CommandBuffer& commandBuffer() { return m_commandBuffer; }
		vk::raii::DescriptorSet& descriptorSet() { return m_descriptorSet; }
		vk::raii::Semaphore&     presentCompleteSemaphore() { return m_presentCompleteSemaphore; }
		vk::raii::Fence&         inFlightFence() { return m_inFlightFence; }

		// 只分配 set 0(UBO)。贴图的 set 1 归 TextureDescriptorCache ——
		// 它按贴图缓存,和帧槽无关。
		void EnsureDescriptorSet(DescriptorAllocator& descriptorAllocator);

		void EnsureUboCapacity(DescriptorAllocator& descriptorAllocator, uint32_t objectCount);

		void ResetObjects() { m_objectCount = 0; }

		uint32_t PushObjectUbo(const glm::mat4& model, const glm::vec4& color);

		//移动语义--------------
		FrameData(const FrameData&) = delete;
		FrameData& operator=(const FrameData&) = delete;
		FrameData(FrameData&&) = default;
		FrameData& operator=(FrameData&&) = default;
		//-----------------------

	private:
		void CreateUbo(uint32_t capacity);

		Buffer                  m_uniformBuffer;
		vk::raii::DescriptorSet m_descriptorSet = nullptr;
		vk::raii::CommandBuffer m_commandBuffer = nullptr;
		vk::raii::Semaphore     m_presentCompleteSemaphore = nullptr;
		vk::raii::Fence         m_inFlightFence = nullptr;

		VulkanContext* m_context = nullptr;

		// 整个环形缓冲在创建时映射一次
		void*    m_uboMappedBase = nullptr;
		uint32_t m_uboStride = 0;      // 对齐后的每物体跨度
		uint32_t m_uboCapacity = 0;    // 能装几个物体
		uint32_t m_objectCount = 0;    // 本帧写到第几个
	};

	class Frames
	{
	public:
		Frames() = default;
		Frames(uint32_t frameCount, VulkanContext* context, CommandPool& commandPool);

		FrameData&       current() { return m_frames[m_current]; }

		void advance() { m_current = (m_current + 1) % static_cast<uint32_t>(m_frames.size()); }

		void EnsureDescriptorSets(DescriptorAllocator& descriptorAllocator);

		void EnsureUboCapacities(DescriptorAllocator& descriptorAllocator, uint32_t objectCount);

	private:
		std::vector<FrameData> m_frames;
		uint32_t               m_current = 0;
	};
}
