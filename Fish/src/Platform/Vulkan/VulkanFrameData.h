#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "VulkanBuffer.h"

#include <vector>
#include <cstdint>

namespace Fish {
	class VulkanContext;
	class CommandPool;
	class DescriptorAllocator;

	// 一次飞行中的帧所需的全部资源 —— 一个 FrameData 就是一个可复用的"帧槽"。
	//
	// 这 5 个的份数必须一致、下标必须一致,因为它们在同一次 submit 里配对:
	//   commandBuffer 被提交
	//   presentCompleteSemaphore 是它等的(图到手了才开始画)
	//   inFlightFence 是它点亮、给 CPU 等的
	//   uniformBuffer 是它的描述符集指向的
	// 换句话说:这 5 个不能分开管。
	//
	// 不在这里的:renderFinishedSemaphores —— 那个按 swapchain image 分,
	// 理由见 SwapChain::createRenderFinishedSemaphores 的注释。
	class FrameData
	{
	public:
		FrameData() = default;
		FrameData(VulkanContext* context,
			CommandPool& commandPool,
			DescriptorAllocator& descriptorAllocator,
			vk::raii::ImageView& textureView,
			vk::raii::Sampler& textureSampler);
		~FrameData() = default;

		vk::raii::CommandBuffer& commandBuffer() { return m_commandBuffer; }
		Buffer&                  uniformBuffer() { return m_uniformBuffer; }
		vk::raii::DescriptorSet& descriptorSet() { return m_descriptorSet; }
		vk::raii::Semaphore&     presentCompleteSemaphore() { return m_presentCompleteSemaphore; }
		vk::raii::Fence&         inFlightFence() { return m_inFlightFence; }

		//移动语义--------------
		FrameData(const FrameData&) = delete;
		FrameData& operator=(const FrameData&) = delete;
		FrameData(FrameData&&) = default;
		FrameData& operator=(FrameData&&) = default;
		//-----------------------

	private:
		//声明顺序,销毁逆序。buffer 先于 descriptorSet 声明,保证 set 先走 ——
		//set 的析构会调 vkFreeDescriptorSets,它需要的是 pool 还活着,
		//而 pool 在 FrameData 外面(见 TriangleApp.h 里 frames 的声明位置注释)。
		Buffer                  m_uniformBuffer;
		vk::raii::DescriptorSet m_descriptorSet = nullptr;
		vk::raii::CommandBuffer m_commandBuffer = nullptr;
		vk::raii::Semaphore     m_presentCompleteSemaphore = nullptr;
		vk::raii::Fence         m_inFlightFence = nullptr;
	};

	// N 份 FrameData + "当前在哪一份"。
	//
	// frameIndex 住在这里而不是散在调用方:拿不到下标就拿到帧,拿到帧就拿到下标,
	// 两者不可能不同步。轮转也因此变成一个必须显式调用的 advance(),
	// 不再是藏在 drawFrame 末尾的一行赋值。
	class Frames
	{
	public:
		Frames() = default;
		Frames(uint32_t frameCount, VulkanContext* context,
			CommandPool& commandPool,
			DescriptorAllocator& descriptorAllocator,
			vk::raii::ImageView& textureView,
			vk::raii::Sampler& textureSampler);

		FrameData&       current() { return m_frames[m_current]; }
		const FrameData& current() const { return m_frames[m_current]; }

		void advance() { m_current = (m_current + 1) % static_cast<uint32_t>(m_frames.size()); }

		uint32_t frameCount() const { return static_cast<uint32_t>(m_frames.size()); }

	private:
		std::vector<FrameData> m_frames;
		uint32_t               m_current = 0;
	};
}
