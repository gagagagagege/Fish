#include"fspch.h"
#include"Renderer.h"
#include"Platform/OpenGL/OpenGLShader.h"
#include"Platform/OpenGL/OpenGLRendererAPI.h"
#include"Platform/Vulkan/VulkanRendererAPI.h"

namespace Fish {
	RendererAPI* Renderer::s_RendererAPI = nullptr;

	Renderer::SceneData* Renderer::s_SceneData = new Renderer::SceneData;

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

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();
		std::dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
		std::dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("u_Transform", transform);

		vertexArray->Bind();
		s_RendererAPI->DrawIndexed(vertexArray);
	}

	void Renderer::SetClearColor(const glm::vec4& color)
	{
		s_RendererAPI->SetClearColor(color);
	}

	void Renderer::DrawFrame()
	{
		s_RendererAPI->DrawFrame();
	}

	void Renderer::NotifyWindowResized()
	{
		s_RendererAPI->NotifyWindowResized();
	}

	VulkanContext* Renderer::GetDeviceContext()
	{
		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		return vulkan ? vulkan->m_DeviceContext.get() : nullptr;
	}
}
