#pragma once
#include"RendererAPI.h"
#include"Renderer/Camera.h"
#include"Shader.h"
#include"Texture.h"
#include"Instance.h"
#include"DrawBatch.h"
#include"Light.h"
#include"glm/glm.hpp"

#include <array>
#include <string>
#include <vector>

namespace Fish {
	class Renderer {
	public:
		static void Init(void* nativeWindow);

		static void Shutdown();

		// 只登记指针,VP 在 DrawFrame 里每帧取一次。相机要活得比 DrawFrame 久。
		// 正常情况下每个相机调一次;重复调会追加,id 不会复用。
		static CameraId AddCamera(Camera& camera);

		// 这个 pass 用哪个相机。id 是 AddCamera 的返回值。
		static void SetPassCamera(RenderPass pass, CameraId camera);

		// 方向光跨帧保留:不调就沿用上一次的值,SceneData 里有一份默认光。
		static void SetLight(const Light& light);

		static void EndScene();
		static void Submit(const Instance& instance);

		static void BatchingSubmit(DrawBatch batch);

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
			SceneData()
			{
				PassCameras.fill(k_InvalidCameraId);
			}

			std::vector<Camera*>                    Cameras;
			std::array<CameraId, k_RenderPassCount> PassCameras;
			Light                                   CurrentLight;
		};

		static SceneData* s_SceneData;

		static RendererAPI* s_RendererAPI;

		static std::vector<Instance> s_DrawQueue;

	};
}