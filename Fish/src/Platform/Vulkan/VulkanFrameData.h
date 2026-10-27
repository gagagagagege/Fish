#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "VulkanBuffer.h"

#include "glm/glm.hpp"

#include <cstddef>
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
		uint32_t  texId;     // 图像表的槽位
		uint32_t  samplerId; // 采样器表的槽位
	};

	// 字段顺序和 offset 不能改:texture.slang / flat_color.slang 里那份 ObjectUbo
	// 必须逐字段对齐。而 AddShaderBindings 只比 set/binding/type,**不比 block 大小**,
	// 错开是查不出来的 —— 这三条是唯一拦得住的地方。
	static_assert(offsetof(ObjectUbo, color) == 64, "ObjectUbo.color 不在 offset 64");
	static_assert(offsetof(ObjectUbo, texId) == 80, "ObjectUbo.texId 不在 offset 80");
	static_assert(offsetof(ObjectUbo, samplerId) == 84, "ObjectUbo.samplerId 不在 offset 84");

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

		// 只分配 set 0(UBO)。贴图那张表只有一份,归 VulkanRendererAPI ——
		// 它和帧槽无关。
		void EnsureDescriptorSet(DescriptorAllocator& descriptorAllocator);

		void EnsureUboCapacity(DescriptorAllocator& descriptorAllocator, uint32_t objectCount);

		void ResetObjects() { m_objectCount = 0; }

		uint32_t PushObjectUbo(const glm::mat4& model, const glm::vec4& color,
			uint32_t texId, uint32_t samplerId);

		FrameData(const FrameData&) = delete;
		FrameData& operator=(const FrameData&) = delete;
		FrameData(FrameData&&) = default;
		FrameData& operator=(FrameData&&) = default;

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

		// 批的顶点缓冲每槽一份,写的时候要挑当前这槽 —— 见 VulkanRendererAPI::BatchBuffer。
		uint32_t         currentIndex() const { return m_current; }

		void advance() { m_current = (m_current + 1) % static_cast<uint32_t>(m_frames.size()); }

		void EnsureDescriptorSets(DescriptorAllocator& descriptorAllocator);

		void EnsureUboCapacities(DescriptorAllocator& descriptorAllocator, uint32_t objectCount);

	private:
		std::vector<FrameData> m_frames;
		uint32_t               m_current = 0;
	};
}
