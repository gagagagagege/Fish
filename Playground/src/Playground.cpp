#include"imgui.h"
#include"includings.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

#include"playground2D.h"
#include"Core/EntryPoint.h"

namespace {
	// 批纹理库的槽
	constexpr uint32_t k_SlotCheckerboard = 0;
	constexpr uint32_t k_SlotLogo         = 1;
	constexpr uint32_t k_SlotWhite        = 2;
	constexpr uint32_t k_BatchTextureCount = 3;

	// 上下键各按一次改这么多。边沿触发,按住不放不会连着涨。
	constexpr uint32_t k_SpriteCountStep = 100;
	constexpr uint32_t k_InitialSpriteCount = 400;   // 改之前那两重循环是 20×20

	constexpr uint32_t k_ReportIntervalFrames = 120;
}

class ExampleLayer :public Fish::Layer {
public:
	ExampleLayer() :Layer("Example"), m_CameraController(1280.0f / 720.0f) {
		const std::string shaderDir = "Fish/src/Platform/Vulkan/shaders/";
		m_FlatColorShader = Fish::Renderer::CreateShader("FlatColor", shaderDir + "flat_color.spv");
		m_TextureShader   = Fish::Renderer::CreateShader("Texture",   shaderDir + "texture.spv");
		m_BatchShader     = Fish::Renderer::CreateShader("Batch",     shaderDir + "batch.spv");
		//flat color
		float triangleVertices[3 * 5] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.0f,  0.5f, 0.0f, 0.5f, 1.0f
		};
		uint32_t triangleIndices[3] = { 0, 1, 2 };
		m_TriangleVB = Fish::Renderer::CreateVertexBuffer(
			triangleVertices, sizeof(triangleVertices), 5 * sizeof(float));
		m_TriangleIB = Fish::Renderer::CreateIndexBuffer(triangleIndices, 3);

		// 方块:pos3 + uv2,stride 20
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

		m_Texture     = Fish::Renderer::CreateTexture2D("Playground/assets/textures/Checkerboard.png", k_SlotCheckerboard);
		m_LogoTexture = Fish::Renderer::CreateTexture2D("Playground/assets/textures/ChernoLogo.png",  k_SlotLogo);
		m_WhiteTexture = Fish::Renderer::CreateTexture2D("Playground/assets/textures/White.png",      k_SlotWhite);
	}

	void OnUpdate(Fish::Timestep ts)override {
		// Update
		m_CameraController.OnUpdate(ts);
		AdjustSpriteCount();
		ReportFrameTime(ts);

		// Render
		Fish::Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });

		Fish::Renderer::BeginScene(m_CameraController.GetCamera());

		const glm::vec4 color = glm::vec4(m_SquareColor, 1.0f);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		const uint32_t columns = std::max(1u, static_cast<uint32_t>(
			std::ceil(std::sqrt(static_cast<double>(m_SpriteCount)))));

		for (uint32_t i = 0; i < m_SpriteCount; ++i) {
			const glm::vec3 pos(static_cast<float>(i % columns) * 0.11f,
			                    static_cast<float>(i / columns) * 0.11f, 0.0f);
			Fish::Renderer::BatchingSubmit({
				.shape        = Fish::BatchShape::Quad,
				.shader       = m_BatchShader,
				.transform    = glm::translate(glm::mat4(1.0f), pos) * scale,
				.color        = color,
				// 三张轮着来
				.texId        = i % k_BatchTextureCount,
				.tilingFactor = 1.0f });
		}

		Fish::Renderer::Submit({
			.shader       = m_TextureShader,
			.vertexBuffer = m_SquareVB,
			.indexBuffer  = m_SquareIB,
			.texture      = m_Texture,
			.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(-0.3f, -0.5f, 0.0f))
			                * glm::scale(glm::mat4(1.0f), glm::vec3(0.8f)) });
		Fish::Renderer::Submit({
			.shader       = m_TextureShader,
			.vertexBuffer = m_SquareVB,
			.indexBuffer  = m_SquareIB,
			.texture      = m_LogoTexture,
			.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(0.6f, -0.5f, 0.0f))
			                * glm::scale(glm::mat4(1.0f), glm::vec3(0.8f)) });

		// 3. 三角形:和上面两条同 stride、同 shader,复用同一条管线。
		Fish::Renderer::Submit({
			.shader       = m_TextureShader,
			.vertexBuffer = m_TriangleVB,
			.indexBuffer  = m_TriangleIB,
			.texture      = m_WhiteTexture,
			.transform    = glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, -0.5f, 0.0f)),
			.color        = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) });

		Fish::Renderer::EndScene();
	}

	virtual void OnImGuiRender() override
	{
		ImGui::Begin("Settings");
		ImGui::ColorEdit3("Square Color", glm::value_ptr(m_SquareColor));
		ImGui::End();
	}

	void OnEvent(Fish::Event& e)override {
		m_CameraController.OnEvent(e);
	}

	// 上下键改精灵数量。屏幕上没有数字可看(M6 才有 ImGui),所以改到哪儿
	// 只能打在 stdout 上。边沿触发:按住不放不会连着涨,想加到 1000 按 6 下。
	// 不能用 KeyPressedEvent 的 repeat —— 那个受窗口层的按键重复设置影响,
	// 而这里要的是"按一次改一次"。
	void AdjustSpriteCount() {
		const bool up   = Fish::Input::IsKeyPressed(Fish::Key::Up);
		const bool down = Fish::Input::IsKeyPressed(Fish::Key::Down);

		if (up && !m_UpHeld)
			SetSpriteCount(m_SpriteCount + k_SpriteCountStep);
		else if (down && !m_DownHeld)
			SetSpriteCount(m_SpriteCount > k_SpriteCountStep ? m_SpriteCount - k_SpriteCountStep : 0);

		m_UpHeld = up;
		m_DownHeld = down;
	}

	void SetSpriteCount(uint32_t count) {
		m_SpriteCount = count;
		std::cout << "[batch] sprites = " << m_SpriteCount << std::endl;
	}

	//验收帧率
	void ReportFrameTime(Fish::Timestep ts) {
		m_FrameAccumMs += ts.GetMilliseconds();
		if (++m_FrameCount < k_ReportIntervalFrames)
			return;

		const float avg = m_FrameAccumMs / static_cast<float>(m_FrameCount);
		std::cout << "[batch] sprites = " << m_SpriteCount
		          << "  avg " << avg << " ms/frame  "
		          << (avg > 0.0f ? 1000.0f / avg : 0.0f) << " fps" << std::endl;
		m_FrameAccumMs = 0.0f;
		m_FrameCount = 0;
	}

private:
	Fish::Ref<Fish::Shader> m_FlatColorShader;   // 零调用者了,留着没删
	Fish::Ref<Fish::Shader> m_TextureShader;
	Fish::Ref<Fish::Shader> m_BatchShader;
	Fish::Ref<Fish::Texture2D> m_WhiteTexture;
	Fish::Ref<Fish::VertexBuffer> m_SquareVB;
	Fish::Ref<Fish::IndexBuffer>  m_SquareIB;
	Fish::Ref<Fish::VertexBuffer> m_TriangleVB;
	Fish::Ref<Fish::IndexBuffer>  m_TriangleIB;
	Fish::Ref<Fish::Texture2D> m_Texture, m_LogoTexture;

	Fish::OrthographicCameraController m_CameraController;

	uint32_t m_SpriteCount = k_InitialSpriteCount;
	bool     m_UpHeld = false;
	bool     m_DownHeld = false;
	float    m_FrameAccumMs = 0.0f;
	uint32_t m_FrameCount = 0;

	glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
};

class Playground :public Fish::Application {
public:
	Playground() {
		PushLayer(new ExampleLayer());
		PushLayer(new playground2D());
	}
	~Playground()
	{}
};

Fish::Application* Fish::CreateApplication() {
	return new Playground;
}
