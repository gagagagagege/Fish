#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "VulkanImage.h"

namespace Fish {
	class VulkanContext;
	class CommandPool;

	class texture
	{
	public:
		texture() = default;
		static texture loadFromFile(VulkanContext* context, CommandPool& transientPool, const char* path);

		Image&               getImage() { return m_image; }
		// VulkanTexture2D::GetWidth() 是 const 的,而 texture 挂在它身上 ——
		// 没有这个重载,const 成员函数里调不到 getImage()。
		const Image&         getImage() const { return m_image; }
		vk::raii::ImageView& getView() { return m_image.getView(); }
		vk::raii::Sampler&   getSampler() { return m_sampler; }

	private:
		static vk::raii::Sampler createSampler(vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device);

	private:
		Image m_image;
		vk::raii::Sampler m_sampler = nullptr;
	};
}
