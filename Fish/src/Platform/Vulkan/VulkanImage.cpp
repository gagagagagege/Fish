#include "VulkanImage.h"

#include "VulkanBuffer.h"
#include "VulkanContext.h"

#include <stdexcept>

namespace Fish {
	Image::Image(vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage,
		const vk::MemoryPropertyFlags& properties, VulkanContext* context)
		: m_extent(extent), m_format(format)
	{
		// tiling 固定 eOptimal:CPU 摸不到,所以上传必须经 staging buffer。
		// mipLevels/arrayLayers/samples 暂固定,等做 mipmap/cubemap/MSAA 时再开参数。
		vk::ImageCreateInfo imageInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = { extent.width, extent.height, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive };
		m_image = vk::raii::Image(context->device, imageInfo);

		vk::MemoryRequirements memRequirements = m_image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
										 .memoryTypeIndex = Buffer::findMemoryType(memRequirements.memoryTypeBits, properties, context->physicalDevice) };
		m_memory = vk::raii::DeviceMemory(context->device, allocInfo);
		m_image.bindMemory(m_memory, 0);

		m_view = createView(m_image, format, context->device);
	}

	vk::raii::ImageView Image::createView(vk::Image const& image, vk::Format format, vk::raii::Device& device)
	{
		vk::ImageViewCreateInfo viewInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1} };
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
