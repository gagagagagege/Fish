#pragma once
#include"RendererAPI.h"
#include"Renderer/OrthographicCamera.h"
#include"Object.h"
#include"Shader.h"
#include"glm/glm.hpp"

namespace Fish {
	class VulkanContext;

	class Renderer {
	public:
		static void Init(void* nativeWindow);

		static void Shutdown();

		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();
		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));

		static void SetClearColor(const glm::vec4& color);

		static void DrawFrame();

		static void NotifyWindowResized();
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

		static VulkanContext* GetDeviceContext();
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* s_SceneData;

		static RendererAPI* s_RendererAPI;

	};
}