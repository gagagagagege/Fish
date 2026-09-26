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

	class Buffer
	{
	public:
		Buffer() = default;
		Buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags& properties, VulkanContext* context);
		~Buffer() { unmap(); }

		void copyBuffer(const vk::raii::Buffer& srcBuffer, CommandPool& transientPool);

		static Buffer createDeviceLocal(const void* data, vk::DeviceSize size,
			vk::BufferUsageFlags usage, VulkanContext* context, CommandPool& transientPool);

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
