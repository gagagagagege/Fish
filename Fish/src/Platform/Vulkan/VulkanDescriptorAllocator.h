#pragma once

#define VK_USE_PLATFORM_WIN32_KHR

//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include<glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include"vulkan/vulkan.h"
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>

#include "VulkanBuffer.h"
#include "VulkanContext.h"

namespace Fish {
	struct UniformBufferObject {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	// 设备级、全局唯一的那一份描述符设施:layout + pool,以及从池里分配集合。
	// 和"每帧飞行几份"无关 —— maxSets 只在构造时收一次。
	//
	// 按寿命切,不按"是什么"切:池和 layout 活到程序结束,描述符集跟着帧槽走
	// (那部分归 FrameData)。
	class DescriptorAllocator
	{
	public:
		DescriptorAllocator() = default;
		DescriptorAllocator(VulkanContext* context, uint32_t maxSets);

		DescriptorAllocator(const DescriptorAllocator&) = delete;
		DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
		DescriptorAllocator(DescriptorAllocator&&) = default;
		DescriptorAllocator& operator=(DescriptorAllocator&&) = default;

		// 分配并写好一个描述符集:binding 0 = UBO,binding 1 = 贴图。
		// 两个资源的寿命不同(UBO 每帧一份,贴图全局唯一),但描述符集本身
		// 只认"此刻这两个句柄是什么",所以必须一次写完 ——
		// 拆成两次 update 会留下一个只写了一半的中间态。
		vk::raii::DescriptorSet allocate(Buffer& ubo,
			vk::raii::ImageView& textureView, vk::raii::Sampler& textureSampler) const;

		vk::raii::DescriptorSetLayout& layout() { return m_layout; }

	private:
		// 和所有 set 一样,layout 和 pool 都必须活到最后一个 set 释放之后。
		// 释放描述符集会调 vkFreeDescriptorSets,它要的是 pool 还活着;
		// 那条约束落在 TriangleApp 的成员顺序上(descriptorAllocator 早于 frames)。
		vk::raii::DescriptorSetLayout m_layout = nullptr;
		vk::raii::DescriptorPool      m_pool = nullptr;
		VulkanContext*                    m_context = nullptr;
	};

	// 每帧那点 CPU 活:算 MVP 写进这一帧的 UBO。
	// 不碰任何成员,所以是自由函数,不是 DescriptorAllocator 的静态方法。
	void updateUniformBuffer(const vk::Extent2D& extent, Buffer& ubo);
}
