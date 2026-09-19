#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include "vulkan/vulkan.h"
#include "glm/glm.hpp"

#include <array>
#include <utility>
#include <vector>

namespace Fish {
	class VulkanContext;
	class CommandPool;

	struct Vertex {
		glm::vec2 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		// TODO(重构): 这两个描述的是渲染侧的顶点布局,不属于通用 Buffer 的职责。
		// 暂时寄放在这里以便 vertexBuffer 类下线; 等 Vertex/mesh 独立成头文件后再迁走。
		static vk::VertexInputBindingDescription getBindingDescription();
		static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
	};

	class Buffer
	{
	public:
		// 空 Buffer: 句柄为 null,可被移动赋值覆盖,析构安全。
		// 用于 TriangleApp 这类需要先声明、后初始化的成员。
		Buffer() = default;
		Buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags& properties, VulkanContext* context);
		~Buffer() { unmap(); }

		void copyBuffer(const vk::raii::Buffer& srcBuffer, CommandPool& transientPool);

		static Buffer createVertexBuffer(const std::vector<Vertex>& vertices, VulkanContext* context, CommandPool& transientPool);
		static Buffer createIndexBuffer(const std::vector<uint16_t>& indices, VulkanContext* context, CommandPool& transientPool);

		// 内存类型查询只依赖物理设备,不依赖 context,故保持裸句柄入参(texture 侧也在用)
		static uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, vk::raii::PhysicalDevice& physicalDevice);
		
		vk::raii::Buffer& getHandle() { return m_buffer; }
		const vk::raii::Buffer& getHandle() const { return m_buffer; }
		vk::raii::DeviceMemory& getMemory() { return m_memory; }
		vk::DeviceSize getSize() { return m_size; }

		void* map(vk::DeviceSize offset, vk::DeviceSize size);
		void unmap();


		//移动语义--------------
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;
		inline Buffer(Buffer&& other) noexcept:m_memory(std::move(other.m_memory)),m_buffer(std::move(other.m_buffer)),
			m_size(other.m_size),m_context(other.m_context),m_mapPtr(std::exchange(other.m_mapPtr, nullptr))
		{
			other.m_context = nullptr;
		}
		inline Buffer& operator=(Buffer&& other) noexcept {
			if (this != &other) {
				unmap();
				m_buffer = std::move(other.m_buffer);
				m_memory = std::move(other.m_memory);
				m_size = other.m_size;
				m_context = other.m_context;
				m_mapPtr = std::exchange(other.m_mapPtr, nullptr);
			}
			return *this;
		}
		//-----------------------


	private:
		vk::raii::DeviceMemory m_memory = nullptr;
		vk::raii::Buffer m_buffer = nullptr;
		vk::DeviceSize m_size = 0;
		VulkanContext* m_context = nullptr;

		void* m_mapPtr = nullptr;
	};
}
