#pragma once
#include"RendererAPI.h"
#include"Renderer/Camera.h"
#include"Shader.h"
#include"Texture.h"
#include"Instance.h"
#include"DrawBatch.h"
#include"Light.h"
#include"glm/glm.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace Fish {
	class Renderer {
	public:
		static void Init(void* nativeWindow);

		static void Shutdown();

		// 键由调用方定("彩色相机"、"光源相机1")。存的是指针,相机要活得比这一帧久。
		static void AddCamera(const std::unordered_map<std::string, Camera*>& cameras);

		// 方向光跨帧保留:不调就沿用上一次的值,SceneData 里有一份默认光。
		static void SetLight(const Light& light);

		static void EndScene();
		static void Submit(const Instance& instance, const std::string& camera);

		static void BatchingSubmit(DrawBatch batch, const std::string& camera);

		static Ref<Shader>       CreateShader(const std::string& name, const std::string& spvPath);

		// 顶点/索引写进后端的大 buffer,返回 mesh 表里的下标。
		static MeshId CreateMesh(const MeshVertex* vertices, uint32_t vertexCount,
			const uint32_t* indices, uint32_t indexCount);

		static Ref<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, uint32_t stride);
		static Ref<IndexBuffer>  CreateIndexBuffer(const uint32_t* indices, uint32_t count);

		static Ref<Texture2D>    CreateTexture2D(const std::string& path);

		static void SetClearColor(const glm::vec4& color);

		static void DrawFrame();

		static void NotifyWindowResized();

		// 等 GPU 空转。销毁应用层持有的 GPU 资源之前调,RendererAPI::WaitIdle。
		static void WaitIdle();
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	private:
		struct SceneData
		{
			std::unordered_map<std::string, Camera*> Cameras;
			Light CurrentLight;
		};

		// 找不到键时断言。Release 下断言是空的,这一条还没兜底。
		static const Camera& GetCamera(const std::string& key);

		static SceneData* s_SceneData;

		static RendererAPI* s_RendererAPI;

		static std::vector<RenderInstance> s_DrawQueue;

	};
}