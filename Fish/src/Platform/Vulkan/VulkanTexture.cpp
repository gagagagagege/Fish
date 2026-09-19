#include <stb_image.h>

#include "VulkanTexture.h"

#include "VulkanBuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanContext.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace Fish {
	// 通道数写一处,和 stbi_load 的 desired_channels、imageSize 的乘法共用
	constexpr int channels = STBI_rgb_alpha;

	texture texture::loadFromFile(VulkanContext* context, CommandPool& transientPool, const char* path)
	{
		int texWidth = 0, texHeight = 0, texChannels = 0;
		stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, channels);
		if (!pixels)
		{
			throw std::runtime_error(std::string("failed to load texture image: ") + path);
		}
		// 接住像素,保证下面任何一步抛异常都能释放
		const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixelGuard(pixels, &stbi_image_free);

		const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(texWidth) * texHeight * channels;

		Buffer stagingBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, context);

		memcpy(stagingBuffer.map(0, imageSize), pixels, imageSize);
		stagingBuffer.unmap();

		texture tex;
		tex.m_image = Image(vk::Extent2D{ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight) },
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal, context);

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands(context, transientPool);
		tex.m_image.transitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		tex.m_image.copyFrom(commandBuffer, stagingBuffer.getHandle(),
			static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		tex.m_image.transitionLayout(commandBuffer, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		endSingleTimeCommands(context, commandBuffer);

		tex.m_sampler = createSampler(context->physicalDevice, context->device);
		return tex;
	}

	vk::raii::Sampler texture::createSampler(vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device)
	{
		// mipLevels = 1,所以 maxLod = 0
		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		vk::SamplerCreateInfo        samplerInfo{ .magFilter = vk::Filter::eLinear,
												 .minFilter = vk::Filter::eLinear,
												 .mipmapMode = vk::SamplerMipmapMode::eLinear,
												 .addressModeU = vk::SamplerAddressMode::eRepeat,
												 .addressModeV = vk::SamplerAddressMode::eRepeat,
												 .addressModeW = vk::SamplerAddressMode::eRepeat,
												 .mipLodBias = 0.0f,
												 .anisotropyEnable = vk::True,
												 .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
												 .compareEnable = vk::False,
												 .compareOp = vk::CompareOp::eAlways,
												 .minLod = 0.0f,
												 .maxLod = 0.0f,
												 .borderColor = vk::BorderColor::eIntOpaqueBlack,
												 .unnormalizedCoordinates = vk::False };

		return vk::raii::Sampler(device, samplerInfo);
	}
}
