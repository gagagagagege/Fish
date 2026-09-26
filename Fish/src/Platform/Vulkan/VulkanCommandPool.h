#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "VulkanSwapChain.h"

#include <cstdint>
#include <vector>

namespace Fish {
	class CommandPool
	{
	public:
		CommandPool() = default;
		explicit CommandPool(VulkanContext* context);

		CommandPool(const CommandPool&) = delete;
		CommandPool& operator=(const CommandPool&) = delete;
		CommandPool(CommandPool&&) = default;
		CommandPool& operator=(CommandPool&&) = default;

		vk::raii::CommandBuffer allocateBuffer() const;

		vk::raii::CommandPool& handle() { return m_pool; }

	private:
		vk::raii::CommandPool m_pool = nullptr;
		VulkanContext*            m_context = nullptr;
	};

	vk::raii::CommandBuffer beginSingleTimeCommands(VulkanContext* context, CommandPool& transientPool);
	void endSingleTimeCommands(VulkanContext* context, vk::raii::CommandBuffer& commandBuffer);
}
