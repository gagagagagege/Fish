#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "VulkanImageDesc.h"

#include <utility>

namespace Fish {
	class VulkanContext;

	// GPU 侧的一张图像:VkImage + 它绑定的显存 + 描述怎么解释它的 view。
	//
	// 两种图由 desc 的类型区分,不传 kind —— 参数不同,重载分得开:
	// TextureDesc 建采样用的色彩图,ShadowMapDesc 建阴影深度图。

	// 交换链的 image 不归这个类管,它们由呈现引擎拥有
	// 借静态 createView 包一层 view,见 SwapChain::createImageViews。
	class Image
	{
	public:
		Image() = default;
		Image(const TextureDesc& desc, VulkanContext* context);
		Image(const ShadowMapDesc& desc, VulkanContext* context);
		~Image() = default;

		// TODO: 深度图的 attachment 布局、按层指定 base/levelCount —— 等阴影 pass 接进来再做。
		// 现在只覆盖贴图上传那两条转换。
		void transitionLayout(vk::raii::CommandBuffer& commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

		// TODO: 逐层拷。现在只写第 0 层,带 mip 的图后面几层不会被填。
		void copyFrom(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Buffer& srcBuffer, uint32_t width, uint32_t height);

		vk::raii::Image&        getHandle() { return m_image; }
		vk::raii::DeviceMemory& getMemory() { return m_memory; }
		vk::raii::ImageView&    getView() { return m_view; }
		vk::Extent2D            getExtent() const { return m_extent; }
		vk::Format              getFormat() const { return m_format; }

		// aspect 由 format 推:深度格式给 eDepth,其余给 eColor。
		// 交换链也调这个函数,所以后两个参数有默认值。
		static vk::raii::ImageView createView(vk::Image const& image, vk::Format format, vk::raii::Device& device,
			uint32_t levelCount = 1, uint32_t layerCount = 1);

		//移动语义--------------
		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;
		inline Image(Image&& other) noexcept : m_memory(std::move(other.m_memory)), m_image(std::move(other.m_image)),
			m_view(std::move(other.m_view)), m_extent(other.m_extent), m_format(other.m_format)
		{
		}
		inline Image& operator=(Image&& other) noexcept {
			if (this != &other) {
				//顺序要紧:view 引用 image,image 绑定 memory,必须先放前者
				m_view = std::move(other.m_view);
				m_image = std::move(other.m_image);
				m_memory = std::move(other.m_memory);
				m_extent = other.m_extent;
				m_format = other.m_format;
			}
			return *this;
		}
		//-----------------------


	private:
		//两个构造函数共用的创建步骤:建图、分配显存、建 view
		void create(vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage,
			uint32_t mipLevels, uint32_t layerCount, VulkanContext* context);

		//声明顺序,销毁逆序
		vk::raii::DeviceMemory m_memory = nullptr;
		vk::raii::Image        m_image = nullptr;
		vk::raii::ImageView    m_view = nullptr;

		vk::Extent2D m_extent{};
		vk::Format   m_format = vk::Format::eUndefined;
	};
}
