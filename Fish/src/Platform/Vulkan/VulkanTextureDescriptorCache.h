#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include <unordered_map>

namespace Fish {
	class VulkanContext;
	class DescriptorAllocator;

	class TextureDescriptorCache
	{
	public:
		TextureDescriptorCache() = default;
		TextureDescriptorCache(VulkanContext* context, DescriptorAllocator& descriptorAllocator);

		vk::raii::DescriptorSet& GetOrCreate(vk::raii::ImageView& view, vk::raii::Sampler& sampler);

	private:
		struct Key
		{
			uint64_t view;
			uint64_t sampler;

			bool operator==(const Key&) const = default;
		};

		struct KeyHash
		{
			size_t operator()(const Key& key) const;
		};

		std::unordered_map<Key, vk::raii::DescriptorSet, KeyHash> m_sets;
		VulkanContext*       m_context = nullptr;
		DescriptorAllocator* m_allocator = nullptr;
	};
}
