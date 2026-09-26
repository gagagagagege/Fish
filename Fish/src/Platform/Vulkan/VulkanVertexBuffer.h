#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "Renderer/Buffer.h"
#include "VulkanBuffer.h"

#include <cstdint>

namespace Fish {
	class VulkanContext;
	class CommandPool;

	// Fish::VertexBuffer 在 Vulkan 上的实现。
	//
	// BufferLayout 在这条路径上不参与建管线 —— 顶点属性是从 shader 反射来的,
	// 调用方只需要把 stride 给对。这两个函数留着只是为了满足接口,
	// 存下来的 layout 没有任何人读。
	//
	// stride 单独存:反射能给出每个属性的 location/format/大小,给不出
	// "一笔顶点占多少字节",而 vkCmdBindVertexBuffers 要的就是它。
	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		// size 用 size_t 而不是 uint32_t:调用方传的几乎总是 sizeof(...),
		// 收窄成 uint32_t 会在 make_shared 那条模板路径上报 C4267。
		VulkanVertexBuffer(VulkanContext* context, CommandPool& transientPool,
			const void* data, size_t size, uint32_t stride);

		const BufferLayout& GetLayout() const override { return m_layout; }
		void SetLayout(const BufferLayout& layout) override { m_layout = layout; }

		const Buffer& buffer() const { return m_buffer; }
		uint32_t      stride() const { return m_stride; }
		// 非索引绘制要用:indexBuffer 为空时 vkCmdDraw 收的是顶点数,
		// 而那个数只有建缓冲的这一方知道。
		uint32_t      count() const { return m_count; }

	private:
		Buffer       m_buffer;
		BufferLayout m_layout;
		uint32_t     m_stride = 0;
		uint32_t     m_count = 0;
	};

	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(VulkanContext* context, CommandPool& transientPool,
			const uint32_t* indices, uint32_t count);

		uint32_t GetCount() const override { return m_count; }

		const Buffer& buffer() const { return m_buffer; }

	private:
		Buffer   m_buffer;
		uint32_t m_count = 0;
	};
}
