#include"playground2D.h"

#include "imgui.h"

#include"PlatForm/OpenGL/OpenGLShader.h"

playground2D::playground2D():Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f)
{
	m_Shader = Fish::Shader::Create(R"(D:\Fish\Playground\assets\shaders\2D_shader.glsl)");
	std::dynamic_pointer_cast<Fish::OpenGLShader>(m_Shader)->Bind();
	std::dynamic_pointer_cast<Fish::OpenGLShader>(m_Shader)->UploadUniformFloat4("u_Color", m_SquareColor);

	float squareVertices[3 * 4] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
	};

	m_SquareVA.reset(Fish::VertexArray::Create());
	Fish::Ref<Fish::VertexBuffer> squareVB;
	squareVB.reset(Fish::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
	squareVB->SetLayout({
		{ Fish::ShaderDataType::Float3, "a_Position" },
		});
	m_SquareVA->AddVertexBuffer(squareVB);

	uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
	Fish::Ref<Fish::IndexBuffer> squareIB;
	squareIB.reset(Fish::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));
	m_SquareVA->SetIndexBuffer(squareIB);
}

void playground2D::OnAttach()
{

}

void playground2D::OnDetach()
{

}

void playground2D::OnEvent(Fish::Event& e)
{

}

void playground2D::OnUpdate(Fish::Timestep ts)
{
	// Update
	m_CameraController.OnUpdate(ts);

	// Render
	Fish::Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	// OPENGL 遗留:Renderer::Clear() 已删(清屏由 BeginFrame 做)。归 W8/W9。
	//Fish::Renderer::Clear();

	Fish::Renderer::BeginScene(m_CameraController.GetCamera());

	Fish::Renderer::Submit(m_Shader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

	Fish::Renderer::EndScene();
}

void playground2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}
