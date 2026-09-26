#include"playground2D.h"

#include "imgui.h"

playground2D::playground2D():Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f)
{
	m_Shader = Fish::Renderer::CreateShader("FlatColor",
		"Fish/src/Platform/Vulkan/shaders/flat_color.spv");

	// pos3,stride 12 —— 又一个和 ExampleLayer 那三条都不同的 stride
	float squareVertices[3 * 4] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
	};
	uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };

	m_SquareVB = Fish::Renderer::CreateVertexBuffer(
		squareVertices, sizeof(squareVertices), 3 * sizeof(float));
	m_SquareIB = Fish::Renderer::CreateIndexBuffer(squareIndices, 6);
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

	Fish::Renderer::BeginScene(m_CameraController.GetCamera());

	// 挪到右下角那条空带子里 —— 原来在原点、scale 1.5,会把 ExampleLayer
	// 那两张贴图方块和三角形整个盖住(没有深度缓冲,只看提交顺序)。
	Fish::Renderer::Submit({
		.shader       = m_Shader,
		.vertexBuffer = m_SquareVB,
		.indexBuffer  = m_SquareIB,
		.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(1.45f, -0.5f, 0.0f))
		                * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)),
		.color        = m_SquareColor });

	Fish::Renderer::EndScene();
}

void playground2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}
