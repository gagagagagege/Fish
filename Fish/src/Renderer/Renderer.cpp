#include"fspch.h"
#include"Renderer.h"
#include"Platform/OpenGL/OpenGLRendererAPI.h"
#include"Platform/Vulkan/VulkanRendererAPI.h"
#include"Platform/Vulkan/VulkanContext.h"
#include"Platform/Vulkan/VulkanShader.h"
#include"Platform/Vulkan/VulkanVertexBuffer.h"
#include"Platform/Vulkan/VulkanTexture2D.h"

#include <map>
#include <utility>

namespace Fish {

	namespace {
		// 同一个 (name, path) 只建一份 shader。
		std::map<std::pair<std::string, std::string>, Ref<Shader>> s_ShaderCache;
	}

	RendererAPI* Renderer::s_RendererAPI = nullptr;

	Renderer::SceneData* Renderer::s_SceneData = new Renderer::SceneData;

	std::vector<DrawItem> Renderer::s_DrawQueue;

	void Renderer::Init(void* nativeWindow)
	{
		// 整个引擎里唯一知道具体后端的地方
		if (s_RendererAPI == nullptr) {
			switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::OpenGL: s_RendererAPI = new OpenGLRendererAPI; break;
			case RendererAPI::API::Vulkan: s_RendererAPI = new VulkanRendererAPI; break;
			default:
				FS_CORE_ASSERT(false, "Unknown RendererAPI!");
				return;
			}
		}
		s_RendererAPI->Init(nativeWindow);
	}

	void Renderer::Shutdown()
	{
		s_ShaderCache.clear();

		// 置空是为了让 Init 里那句 != nullptr 判断保持能重进的形状,单进程只会走一次。
		delete s_RendererAPI;
		s_RendererAPI = nullptr;

		delete s_SceneData;
		s_SceneData = nullptr;
	}

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::EndScene()
	{}

	void Renderer::Submit(DrawItem item)
	{
		item.viewProjection = s_SceneData->ViewProjectionMatrix;
		s_DrawQueue.push_back(std::move(item));
	}

	void Renderer::BatchingSubmit(DrawBatch batch)
	{
		batch.viewProjection = s_SceneData->ViewProjectionMatrix;
		s_RendererAPI->SubmitBatch(batch);
	}

	void Renderer::SetClearColor(const glm::vec4& color)
	{
		s_RendererAPI->SetClearColor(color);
	}

	void Renderer::DrawFrame()
	{
		s_RendererAPI->DrawFrame(s_DrawQueue);
		s_DrawQueue.clear();
	}

	void Renderer::NotifyWindowResized()
	{
		s_RendererAPI->NotifyWindowResized();
	}

	void Renderer::WaitIdle()
	{
		if (s_RendererAPI) {
			s_RendererAPI->WaitIdle();
		}
	}

	Ref<Shader> Renderer::CreateShader(const std::string& name, const std::string& spvPath)
	{
		const std::pair<std::string, std::string> key{ name, spvPath };
		if (const auto it = s_ShaderCache.find(key); it != s_ShaderCache.end())
			return it->second;

		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		FS_CORE_ASSERT(vulkan, "CreateShader 需要 Vulkan 后端");
		if (!vulkan)
			return nullptr;

		Ref<Shader> shader = std::make_shared<VulkanShader>(vulkan->m_DeviceContext.get(),
			vulkan->m_DescriptorAllocator, name, spvPath);
		s_ShaderCache.emplace(key, shader);
		return shader;
	}

	Ref<VertexBuffer> Renderer::CreateVertexBuffer(const void* data, size_t size, uint32_t stride)
	{
		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		FS_CORE_ASSERT(vulkan, "CreateVertexBuffer 需要 Vulkan 后端");
		if (!vulkan)
			return nullptr;

		return std::make_shared<VulkanVertexBuffer>(vulkan->m_DeviceContext.get(),
			vulkan->m_TransientPool, data, size, stride);
	}

	Ref<IndexBuffer> Renderer::CreateIndexBuffer(const uint32_t* indices, uint32_t count)
	{
		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		FS_CORE_ASSERT(vulkan, "CreateIndexBuffer 需要 Vulkan 后端");
		if (!vulkan)
			return nullptr;

		return std::make_shared<VulkanIndexBuffer>(vulkan->m_DeviceContext.get(),
			vulkan->m_TransientPool, indices, count);
	}

	Ref<Texture2D> Renderer::CreateTexture2D(const std::string& path, uint32_t batchSlot)
	{
		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		FS_CORE_ASSERT(vulkan, "CreateTexture2D 需要 Vulkan 后端");
		if (!vulkan)
			return nullptr;

		auto texture = std::make_shared<VulkanTexture2D>(vulkan->m_DeviceContext.get(),
			vulkan->m_TransientPool, path);

		// 后端的纹理库按槽位记 view/sampler,画批之前一次写进 set 2。
		if (batchSlot != k_NoBatchTextureSlot)
			vulkan->RegisterBatchTexture(batchSlot, texture->getTexture());

		return texture;
	}
}
