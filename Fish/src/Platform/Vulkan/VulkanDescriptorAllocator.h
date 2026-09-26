#pragma once

#define VK_USE_PLATFORM_WIN32_KHR

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <vector>

#include "VulkanContext.h"
#include "VulkanReflection.h"

namespace Fish {
	// 写入侧的约定。layout 不从这里来 —— 它是反射出来的。
	// set 的划分见 texture.slang 的注释。
	inline constexpr uint32_t k_ObjectUboSet     = 0;
	inline constexpr uint32_t k_ObjectUboBinding = 0;
	inline constexpr uint32_t k_TextureSet       = 1;
	inline constexpr uint32_t k_TextureBinding   = 0;

	inline constexpr uint32_t k_DescriptorSetBudget = 128;

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator() = default;

		DescriptorAllocator(const DescriptorAllocator&) = delete;
		DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
		DescriptorAllocator(DescriptorAllocator&&) = default;
		DescriptorAllocator& operator=(DescriptorAllocator&&) = default;

		void AddShaderBindings(VulkanContext* context,
			const std::vector<ReflectedDescriptorBindings::Binding>& bindings);

		bool hasLayout() const { return !m_layouts.empty(); }

		vk::raii::DescriptorSet allocateSet(uint32_t set);

		void writeUniformBuffer(vk::raii::DescriptorSet& set, uint32_t binding,
			const vk::DescriptorBufferInfo& info) const;
		void writeCombinedImageSampler(vk::raii::DescriptorSet& set, uint32_t binding,
			const vk::DescriptorImageInfo& info) const;

		// 按 set 号升序的整套 layout。建 pipeline layout 时整份拿去。
		// 非 const:调一次就把 layout 冻住 —— 交出去的是裸句柄,调用方会一直留着。
		std::vector<vk::DescriptorSetLayout> layoutHandles();

	private:
		void BuildLayout();
		void BuildPool(uint32_t maxSets);

		std::vector<vk::raii::DescriptorSetLayout> m_layouts;
		vk::raii::DescriptorPool      m_pool = nullptr;
		VulkanContext*                m_context = nullptr;

		// layout 一旦被引用出去就不能再重建:裸句柄会悬空,而 pipeline / set
		// 里引用的 VkDescriptorSetLayout 更不会自己更新。
		// 两条置位路径:allocateSet(池建出来 = 已经有 set 引用它了)、
		// layoutHandles / layout(句柄交出去了)。
		bool m_layoutFrozen = false;

		std::vector<ReflectedDescriptorBindings::Binding> m_bindings;
	};
}
