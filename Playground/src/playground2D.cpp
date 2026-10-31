#include"playground2D.h"

#include "imgui.h"

namespace {
	// 和 Playground.cpp 用同一个键 —— 两个 layer 各存各的 map,键只在本层里查。
	const std::string k_ColorCamera = "彩色相机";
}

playground2D::playground2D():Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f)
{
	m_Shader = Fish::Renderer::CreateShader("Texture",
		"Fish/src/Platform/Vulkan/shaders/texture.spv");

	float squareVertices[5 * 4] = {
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
	};
	uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };

	m_SquareVB = Fish::Renderer::CreateVertexBuffer(
		squareVertices, sizeof(squareVertices), 5 * sizeof(float));
	m_SquareIB = Fish::Renderer::CreateIndexBuffer(squareIndices, 6);

	m_WhiteTexture = Fish::Renderer::CreateTexture2D("Playground/assets/textures/White.png");
}

void playground2D::OnAttach()
{

}

void playground2D::OnDetach()
{

}

void playground2D::OnEvent(Fish::Event& e)
{
	m_CameraController.OnEvent(e);
}

void playground2D::OnUpdate(Fish::Timestep ts)
{
	// Update
	m_CameraController.OnUpdate(ts);

	// Render
	Fish::Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });

	Fish::Renderer::AddCamera({ { k_ColorCamera, &m_CameraController.GetCamera() } });

	// 挪到右下角那条空带子里 —— 原来在原点、scale 1.5,会把 ExampleLayer
	// 那两张贴图方块和三角形整个盖住(没有深度缓冲,只看提交顺序)。
	Fish::Renderer::Submit({
		.shader       = m_Shader,
		.vertexBuffer = m_SquareVB,
		.indexBuffer  = m_SquareIB,
		.texture      = m_WhiteTexture,
		.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(1.45f, -0.5f, 0.0f))
		                * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)),
		.color        = m_SquareColor }, k_ColorCamera);

	Fish::Renderer::EndScene();
}

void playground2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}
