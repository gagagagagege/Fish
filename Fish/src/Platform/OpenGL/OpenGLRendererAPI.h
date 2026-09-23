#pragma once
#include"Renderer/RendererAPI.h"

namespace Fish {
	class OpenGLRendererAPI :public RendererAPI
	{
	public:
		virtual void Init(void* nativeWindow) override;
		virtual void SetClearColor(const glm::vec4& color) override;
		// OPENGL 遗留:Clear() 和 setViewport() 都已从 RendererAPI 拿掉,
		// 这个类整个是待删的,不再为它补 NotifyWindowResized / DrawFrame。

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) override;
	};
}

