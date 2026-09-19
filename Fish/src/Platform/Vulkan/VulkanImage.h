#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include <utility>

namespace Fish {
	class VulkanContext;

	// GPU 侧的一张图像:VkImage + 它绑定的显存 + 描述怎么解释它的 view。

	// 交换链的 image 不归这个类管,它们由呈现引擎拥有
	// 借静态 createView 包一层 view,见 SwapChain::createImageViews。
	class Image
	{
	public:
		Image() = default;
		Image(vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage,
			const vk::MemoryPropertyFlags& properties, VulkanContext* context);
		~Image() = default;

		void transitionLayout(vk::raii::CommandBuffer& commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
		void copyFrom(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Buffer& srcBuffer, uint32_t width, uint32_t height);

		vk::raii::Image&        getHandle() { return m_image; }
		vk::raii::DeviceMemory& getMemory() { return m_memory; }
		vk::raii::ImageView&    getView() { return m_view; }
		vk::Extent2D            getExtent() const { return m_extent; }
		vk::Format              getFormat() const { return m_format; }

		static vk::raii::ImageView createView(vk::Image const& image, vk::Format format, vk::raii::Device& device);

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
		//声明顺序,销毁逆序
		vk::raii::DeviceMemory m_memory = nullptr;
		vk::raii::Image        m_image = nullptr;
		vk::raii::ImageView    m_view = nullptr;

		vk::Extent2D m_extent{};
		vk::Format   m_format = vk::Format::eUndefined;
	};
}
