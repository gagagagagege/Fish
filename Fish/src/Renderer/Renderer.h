#pragma once
#include"RendererAPI.h"
#include"Renderer/OrthographicCamera.h"
#include"Shader.h"
#include"Texture.h"
#include"DrawItem.h"
#include"glm/glm.hpp"

#include <vector>

namespace Fish {
	class Renderer {
	public:
		static void Init(void* nativeWindow);

		static void Shutdown();

		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();
		// 只入队,不碰命令缓冲 —— 命令缓冲只在后端 DrawFrame 里开着。
		// 每个绘制项自己带齐 shader / 顶点缓冲 / 索引缓冲 / 贴图 / 颜色,
		// 没有"先绑定再画"这一步。
		static void Submit(DrawItem item);

		static Ref<Shader>       CreateShader(const std::string& name, const std::string& spvPath);
		static Ref<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, uint32_t stride);
		static Ref<IndexBuffer>  CreateIndexBuffer(const uint32_t* indices, uint32_t count);
		static Ref<Texture2D>    CreateTexture2D(const std::string& path);

		static void SetClearColor(const glm::vec4& color);

		static void DrawFrame();

		static void NotifyWindowResized();

		// 等 GPU 空转。销毁应用层持有的 GPU 资源之前调,见 RendererAPI::WaitIdle。
		static void WaitIdle();
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* s_SceneData;

		static RendererAPI* s_RendererAPI;

		static std::vector<DrawItem> s_DrawQueue;

	};
}