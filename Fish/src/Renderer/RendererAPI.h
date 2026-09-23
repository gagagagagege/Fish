#pragma once
#include"glm/glm.hpp"
#include "VertexArray.h"

namespace Fish {
	class RendererAPI
	{
	public:
		enum class API {
			None = 0, OpenGL = 1, Vulkan = 2
		};

		// 必须有,而且必须有 virtual。Renderer 存的是指向派生类的 RendererAPI*,
		// 非虚析构下 delete 只跑基类那一层 —— 2026-09-23 实测:去掉 virtual 后
		// 派生类析构不执行、整套 vk::raii 不销毁,而进程照样 exit=0、验证层 0 输出。
		virtual ~RendererAPI() = default;

		// 用 void* 而不是 GLFWwindow* 是为了不把 GLFW 头带进 Renderer/RendererAPI
		virtual void Init(void* nativeWindow) = 0;

		// 原来叫 setViewport 且收四个参数。Vulkan 侧 viewport 是录制时按交换链 extent 设的,
		// 一个参数都用不上,所以改成纯通知。
		virtual void NotifyWindowResized() {}

		virtual void SetClearColor(const glm::vec4& color) = 0;

		// 一帧的全部:acquire -> 录 -> submit -> present。Clear() 没有单独的调用 ——
		// 动态渲染下清屏是 beginRendering 的 clearValue,用 SetClearColor 的值。
		// 非纯虚是为了待删的 OpenGLRendererAPI 还能实例化。
		virtual void DrawFrame() {}

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) = 0;

		static const API GetAPI() { return s_API; }
	private:
		static API s_API;
	};
}


