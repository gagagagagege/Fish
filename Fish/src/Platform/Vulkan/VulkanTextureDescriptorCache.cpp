#include "VulkanTextureDescriptorCache.h"

#include "VulkanContext.h"
#include "VulkanDescriptorAllocator.h"

#include <cstdint>

namespace Fish {
	namespace {

		// 收的是裸句柄(raii 包装不可拷贝)。非分派句柄是个指针或者 uint64_t,
		// 取决于 VK_USE_64_BIT_PTR_DEFINES,reinterpret_cast 两种形态都吃得下。
		template <typename Handle>
		uint64_t HandleKey(Handle handle)
		{
			using Native = typename Handle::NativeType;
			return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(static_cast<Native>(handle)));
		}

	}

	size_t TextureDescriptorCache::KeyHash::operator()(const Key& key) const
	{
		size_t seed = std::hash<uint64_t>{}(key.view);
		seed ^= std::hash<uint64_t>{}(key.sampler) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
		return seed;
	}

	TextureDescriptorCache::TextureDescriptorCache(VulkanContext* context,
		DescriptorAllocator& descriptorAllocator)
		: m_context(context)
		, m_allocator(&descriptorAllocator)
	{
	}

	vk::raii::DescriptorSet& TextureDescriptorCache::GetOrCreate(vk::raii::ImageView& view,
		vk::raii::Sampler& sampler)
	{
		const Key key{ HandleKey(*view), HandleKey(*sampler) };

		auto existing = m_sets.find(key);
		if (existing != m_sets.end())
			return existing->second;

		vk::raii::DescriptorSet set = m_allocator->allocateSet(k_TextureSet);
		m_allocator->writeCombinedImageSampler(set, k_TextureBinding,
			vk::DescriptorImageInfo{
				.sampler = sampler,
				.imageView = view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal });

		auto inserted = m_sets.emplace(key, std::move(set));
		return inserted.first->second;
	}
}
