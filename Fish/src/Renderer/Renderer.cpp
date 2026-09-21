#include"fspch.h"
#include"Renderer.h"
#include"Platform/OpenGL/OpenGLShader.h"
#include"Platform/OpenGL/OpenGLRendererAPI.h"
#include"Platform/Vulkan/VulkanRendererAPI.h"

namespace Fish {
	// 改成运行时创建:建 Vulkan 的 surface 要窗口,静态初始化时 GLFW 还没 init
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

	void Renderer::Clear()
	{
		s_RendererAPI->Clear();
	}

	void Renderer::onWindowResize(uint32_t width, uint32_t height)
	{
		s_RendererAPI->setViewport(0,0,width, height);
	}

	VulkanContext* Renderer::GetDeviceContext()
	{
		// 只读 s_RendererAPI 一处状态。用 RendererAPI::GetAPI() 判断的话就多一处
		// 状态要跟它保持一致(s_API 和 s_RendererAPI 现在都是各写各的)
		auto* vulkan = dynamic_cast<VulkanRendererAPI*>(s_RendererAPI);
		return vulkan ? vulkan->m_DeviceContext.get() : nullptr;
	}
}
