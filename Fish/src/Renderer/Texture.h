#pragma once
#include"Core/macro.h"

namespace Fish {
	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// 空实现而不是纯虚:贴图在描述符模型下是"跟着这次绘制走"的输入,
		// 没有"先绑上再画"。等 Platform/OpenGL/ 整批下线时连这个一起删。
		virtual void Bind(uint32_t slot = 0) const {}

		// 这张贴图在图像表 / 采样器表里的槽位。基类给 0 只是占位 —— 只有
		// VulkanTexture2D 会进 Vulkan 的绘制路径,别的实现读到什么都用不上。
		virtual uint32_t GetBindlessIndex() const { return 0; }
		virtual uint32_t GetSamplerIndex() const { return 0; }
	};

	// 贴图创建走 Renderer::CreateTexture2D,这里不再有静态工厂。
	class Texture2D : public Texture
	{
	public:
		virtual ~Texture2D() = default;
	};
}
