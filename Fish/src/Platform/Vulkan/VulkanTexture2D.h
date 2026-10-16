#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "Renderer/Texture.h"
#include "VulkanTexture.h"

#include <string>

namespace Fish {
	class VulkanContext;
	class CommandPool;

	// Fish::Texture2D 在 Vulkan 上的实现。
	//
	// Bind(slot) 在描述符模型下没有对应物 —— 贴图不是"先绑上再画",而是
	// 描述符集里的一个绑定,哪张贴图跟着绘制项走。基类那个是空实现,这里不重写。
	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(VulkanContext* context, CommandPool& transientPool, const std::string& path);

		uint32_t GetWidth() const override;
		uint32_t GetHeight() const override;

		// 槽位不是构造参数 —— 发号要拿贴图本身的 view/sampler,得等它建完。
		uint32_t GetBindlessIndex() const override { return m_bindlessIndex; }
		void     SetBindlessIndex(uint32_t slot) { m_bindlessIndex = slot; }

		const texture& getTexture() const { return m_texture; }
		texture&       getTexture() { return m_texture; }

	private:
		texture  m_texture;
		uint32_t m_bindlessIndex = 0;
	};
}
