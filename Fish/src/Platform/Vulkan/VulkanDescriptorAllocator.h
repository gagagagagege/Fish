#pragma once

#define VK_USE_PLATFORM_WIN32_KHR

#include <vulkan/vulkan_raii.hpp>

#include <array>
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
	inline constexpr uint32_t k_TextureImageBinding   = 0;   // Texture2D textures[]
	inline constexpr uint32_t k_TextureSamplerBinding = 1;   // SamplerState samplers[]

	// 两张表的格数上界。实际各取 min(设备 limit, 这个数),见 sampledImageTableSize()
	// / samplerTableSize()。shader 写的是运行时数组,不带长度 —— 这些数只出现在
	// layout 里,不用和 shader 对齐(以前 64 要在两处手工对齐)。
	inline constexpr uint32_t k_MaxBindlessTextures = 4096;
	inline constexpr uint32_t k_MaxBindlessSamplers = 256;

	inline constexpr uint32_t k_DescriptorSetBudget = 128;

	// 表只有一套,池也就只要装一个 set。不能沿用 k_DescriptorSetBudget ——
	// updateAfterBind 池的描述符总量受 maxUpdateAfterBindDescriptorsInAllPools
	// 约束,按 128 倍预留会直接建池失败。
	inline constexpr uint32_t k_TextureTableSetCount = 1;

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

		// 两张表各能装几格。layout 的 descriptorCount 和槽位分配器读的是同一个数。
		// 没登记过 shader 时没有设备可查,直接抛。
		uint32_t sampledImageTableSize();
		uint32_t samplerTableSize();

		void writeUniformBuffer(vk::raii::DescriptorSet& set, uint32_t binding,
			const vk::DescriptorBufferInfo& info) const;
		void writeSampledImages(vk::raii::DescriptorSet& set, uint32_t binding,
			uint32_t firstElement, const std::vector<vk::DescriptorImageInfo>& infos) const;
		void writeSamplers(vk::raii::DescriptorSet& set, uint32_t binding,
			uint32_t firstElement, const std::vector<vk::DescriptorImageInfo>& infos) const;

		// 按 set 号升序的整套 layout。建 pipeline layout 时整份拿去。
		// 非 const:调一次就把 layout 冻住 —— 交出去的是裸句柄,调用方会一直留着。
		std::vector<vk::DescriptorSetLayout> layoutHandles();

	private:
		void     BuildLayout();
		void     BuildPool(uint32_t set, uint32_t maxSets);
		uint32_t DescriptorCount(const ReflectedDescriptorBindings::Binding& binding);
		uint32_t ComputeTableSize(vk::DescriptorType type);

		// 哪些 set 走 updateAfterBind 那一套。shader 里表达不了这个 ——
		// SPIR-V 只有 binding 号,没有"这个 binding 可以绑定后再改"。
		static constexpr bool IsUpdateAfterBindSet(uint32_t set) { return set == k_TextureSet; }
		static constexpr uint32_t PoolIndex(uint32_t set) { return IsUpdateAfterBindSet(set) ? 1u : 0u; }

		std::vector<vk::raii::DescriptorSetLayout> m_layouts;
		std::array<vk::raii::DescriptorPool, 2>    m_pools{ nullptr, nullptr };
		VulkanContext* m_context = nullptr;

		// 0 表示还没算过。算一次就够 —— 设备在进程生命周期里不会换。
		uint32_t m_sampledImageTableSize = 0;
		uint32_t m_samplerTableSize      = 0;

		// layout 一旦被引用出去就不能再重建:裸句柄会悬空,而 pipeline / set
		// 里引用的 VkDescriptorSetLayout 更不会自己更新。
		// 两条置位路径:allocateSet(池建出来 = 已经有 set 引用它了)、
		// layoutHandles / layout(句柄交出去了)。
		bool m_layoutFrozen = false;

		std::vector<ReflectedDescriptorBindings::Binding> m_bindings;
	};
}
