#pragma once 
#include"RendererAPI.h"
#include"RenderCommand.h"
#include"Renderer/OrthographicCamera.h"
#include"Object.h"
#include"Shader.h"
#include"glm/glm.hpp"

namespace Fish {
	class Renderer {
	public:
		static void Init();
		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();
		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));

		static void onWindowResize(uint32_t width, uint32_t height);
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* s_SceneData;

	};
}