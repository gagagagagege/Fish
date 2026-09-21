#pragma once
#include"RendererAPI.h"
#include"Renderer/OrthographicCamera.h"
#include"Object.h"
#include"Shader.h"
#include"glm/glm.hpp"

namespace Fish {
	// 前置声明,不 include —— Renderer.h 被 includings.h 收着,
	// 一旦这里出现 vulkan_raii.hpp,Playground 那一侧的 TU 也会看到
	class VulkanContext;

	class Renderer {
	public:
		static void Init(void* nativeWindow);
		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();
		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));

		static void SetClearColor(const glm::vec4& color);
		static void Clear();

		static void onWindowResize(uint32_t width, uint32_t height);
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

		// 资源类的 Create() 工厂(Buffer/Shader/Texture/VertexArray 那 6 处)靠这个
		// 拿到 device。当前后端不是 Vulkan 时返回 nullptr。
		static VulkanContext* GetDeviceContext();
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* s_SceneData;

		// 原来挂在 RenderCommand 上。Fish 只做 Vulkan,中间那层纯转发没有存在理由了。
		static RendererAPI* s_RendererAPI;

	};
}