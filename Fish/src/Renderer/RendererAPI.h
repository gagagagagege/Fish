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

		// 用 void* 而不是 GLFWwindow* 是为了不把 GLFW 头带进 Renderer/RendererAPI
		virtual void Init(void* nativeWindow) = 0;
		virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) = 0;

		static const API GetAPI() { return s_API; }
	private:
		static API s_API;
	};
}


