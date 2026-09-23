#include "fspch.h"
#include "OpenGLRendererAPI.h"
#include"glad/glad.h"


namespace Fish {
	void OpenGLRendererAPI::Init(void* nativeWindow)
	{
		// GL 的设备是线程全局的 current context,窗口在 WindowsWindow::Init 里
		// 由 OpenGLContext 设好了,这里用不上。参数是为 Vulkan 那侧加的。
		(void)nativeWindow;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	// OPENGL 遗留:setViewport 已被 NotifyWindowResized 取代。留着原实现备查。
	//void OpenGLRendererAPI::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	//{
	//	glViewport(x, y, width, height);
	//}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	// OPENGL 遗留:Clear() 已从 RendererAPI 删掉。这里留着原实现备查。
	//void OpenGLRendererAPI::Clear()
	//{
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray)
	{
		glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
	}
}