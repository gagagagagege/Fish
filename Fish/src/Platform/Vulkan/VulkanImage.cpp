#include "VulkanImage.h"

#include "VulkanBuffer.h"
#include "VulkanContext.h"

#include <stdexcept>

namespace Fish {
	namespace {
		vk::Format ToVkFormat(TextureFormat format)
		{
			switch (format) {
			case TextureFormat::RGBA8_SRGB:  return vk::Format::eR8G8B8A8Srgb;
			case TextureFormat::RGBA8_UNORM: return vk::Format::eR8G8B8A8Unorm;
			case TextureFormat::R8_UNORM:    return vk::Format::eR8Unorm;
			}
			throw std::invalid_argument("unknown TextureFormat");
		}

		vk::Format ToVkFormat(ShadowFormat format)
		{
			switch (format) {
			case ShadowFormat::D24_UNORM_S8: return vk::Format::eD24UnormS8Uint;
			case ShadowFormat::D32_SFLOAT:   return vk::Format::eD32Sfloat;
			}
			throw std::invalid_argument("unknown ShadowFormat");
		}

		// 只有深度/深度模板格式走 eDepth。组合格式只建深度 aspect ——
		// 阴影采样读的是深度,模板那半没有采样路径。
		vk::ImageAspectFlags AspectOf(vk::Format format)
		{
			switch (format) {
			case vk::Format::eD16Unorm:
			case vk::Format::eD32Sfloat:
			case vk::Format::eD24UnormS8Uint:
			case vk::Format::eD32SfloatS8Uint:
			case vk::Format::eX8D24UnormPack32:
				return vk::ImageAspectFlagBits::eDepth;
			default:
				return vk::ImageAspectFlagBits::eColor;
			}
		}
	}

	Image::Image(const TextureDesc& desc, VulkanContext* context)
	{
		const vk::Format format = ToVkFormat(desc.format);

		// usage:上传要 TransferDst,采样要 Sampled。Storage 按设备支持加 ——
		// sRGB 格式在不少驱动上没有 STORAGE_IMAGE(本机就没有),无条件带上验证层会报
		// 02251 / 02275。
		//
		// 没有 TransferSrc —— blit 生成 mip 那条路做的时候再加,blit 的源要它。
		vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

		const auto formatProperties =
			context->physicalDevice.getFormatProperties2<vk::FormatProperties2, vk::FormatProperties3>(format);
		if (formatProperties.get<vk::FormatProperties3>().optimalTilingFeatures & vk::FormatFeatureFlagBits2::eStorageImage)
			usage |= vk::ImageUsageFlagBits::eStorage;

		create(vk::Extent2D{ desc.extent.width, desc.extent.height },
			format, usage, desc.mipLevels, 1, context);
	}

	Image::Image(const ShadowMapDesc& desc, VulkanContext* context)
	{
		// 阴影 pass 当附件、主 pass 采样,两个 usage 都要。mip 固定一层。
		create(vk::Extent2D{ desc.extent.width, desc.extent.height },
			ToVkFormat(desc.format),
			vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			1, desc.layerCount, context);
	}

	// 建图、分配显存、建 view。两边的差异全在参数里,这里不分支。
	void Image::create(vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage,
		uint32_t mipLevels, uint32_t layerCount, VulkanContext* context)
	{
		m_extent = extent;
		m_format = format;

		// tiling 固定 eOptimal:CPU 摸不到,所以上传必须经 staging buffer。
		vk::ImageCreateInfo imageInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = { extent.width, extent.height, 1 },
			.mipLevels = mipLevels,
			.arrayLayers = layerCount,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive };
		m_image = vk::raii::Image(context->device, imageInfo);

		vk::MemoryRequirements memRequirements = m_image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
										 .memoryTypeIndex = Buffer::findMemoryType(memRequirements.memoryTypeBits,
											 vk::MemoryPropertyFlagBits::eDeviceLocal, context->physicalDevice) };
		m_memory = vk::raii::DeviceMemory(context->device, allocInfo);
		m_image.bindMemory(m_memory, 0);

		// view 的层数跟图的 mip 层数走,array 层数跟图走 —— 少一样就采不到那部分内容。
		m_view = createView(m_image, format, context->device, mipLevels, layerCount);
	}

	vk::raii::ImageView Image::createView(vk::Image const& image, vk::Format format, vk::raii::Device& device,
		uint32_t levelCount, uint32_t layerCount)
	{
		vk::ImageViewCreateInfo viewInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {.aspectMask = AspectOf(format), .baseMipLevel = 0, .levelCount = levelCount,
				.baseArrayLayer = 0, .layerCount = layerCount} };
		return vk::raii::ImageView(device, viewInfo);
	}

	void Image::transitionLayout(vk::raii::CommandBuffer& commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
	{
		vk::PipelineStageFlags sourceStage{};
		vk::PipelineStageFlags destinationStage{};

		vk::ImageMemoryBarrier barrier{ .oldLayout = oldLayout,
							   .newLayout = newLayout,
							   .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
							   .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
							   .image = m_image,
							   .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1} };
		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}
		commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
	}

	void Image::copyFrom(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Buffer& srcBuffer, uint32_t width, uint32_t height)
	{
		// bufferRowLength / bufferImageHeight 为 0 = 紧密打包,
		// 即 buffer 里按 imageExtent.width 的行宽排列 —— 正是 stb 输出的布局。
		vk::BufferImageCopy region{ .bufferOffset = 0,
						   .bufferRowLength = 0,
						   .bufferImageHeight = 0,
						   .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
						   .imageOffset = {0, 0, 0},
						   .imageExtent = {width, height, 1} };
		commandBuffer.copyBufferToImage(srcBuffer, m_image, vk::ImageLayout::eTransferDstOptimal, region);
	}
}
