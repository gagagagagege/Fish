#include"fspch.h"
#include"Renderer.h"
#include"Platform/OpenGL/OpenGLRendererAPI.h"
#include"Platform/Vulkan/VulkanRendererAPI.h"
#include"Platform/Vulkan/VulkanContext.h"
#include"Platform/Vulkan/VulkanShader.h"
#include"Platform/Vulkan/VulkanVertexBuffer.h"
#include"Platform/Vulkan/VulkanTexture2D.h"

namespace Fish {
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

	// 下面几个都靠 dynamic_cast 拿后端的具体类型。
	// 不往 RendererAPI 上加虚函数:那等于把"引擎有哪几种资源"写进后端接口,
	// 加一种资源就要动所有实现(包括已经没人维护的 OpenGLRendererAPI)。
	Ref<Shader> Renderer::CreateShader(const std::string& name, const std::string& spvPath)
	{
		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		FS_CORE_ASSERT(vulkan, "CreateShader 需要 Vulkan 后端");
		if (!vulkan)
			return nullptr;

		return std::make_shared<VulkanShader>(vulkan->m_DeviceContext.get(),
			vulkan->m_DescriptorAllocator, name, spvPath);
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

	Ref<Texture2D> Renderer::CreateTexture2D(const std::string& path)
	{
		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		FS_CORE_ASSERT(vulkan, "CreateTexture2D 需要 Vulkan 后端");
		if (!vulkan)
			return nullptr;

		return std::make_shared<VulkanTexture2D>(vulkan->m_DeviceContext.get(),
			vulkan->m_TransientPool, path);
	}
}
