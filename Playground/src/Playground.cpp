#include"imgui.h"
#include"includings.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include"playground2D.h"
#include"Core/EntryPoint.h"

// 所有绘制项都走 texture 那条 shader —— 纯色项靠绑一张 1×1 白贴图 +
// ubo.color 出颜色。顶点一律 pos3 + uv2、stride 20,所以全场共用一条管线。
// 管线按 (shader 模块, stride, 渲染状态) 缓存,顶点布局从 shader 反射出来。
class ExampleLayer :public Fish::Layer {
public:
	ExampleLayer() :Layer("Example"), m_CameraController(1280.0f / 720.0f) {
		const std::string shaderDir = "Fish/src/Platform/Vulkan/shaders/";
		m_FlatColorShader = Fish::Renderer::CreateShader("FlatColor", shaderDir + "flat_color.spv");
		m_TextureShader   = Fish::Renderer::CreateShader("Texture",   shaderDir + "texture.spv");
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

		m_Texture     = Fish::Renderer::CreateTexture2D("Playground/assets/textures/Checkerboard.png");
		m_LogoTexture = Fish::Renderer::CreateTexture2D("Playground/assets/textures/ChernoLogo.png");
		m_WhiteTexture = Fish::Renderer::CreateTexture2D("Playground/assets/textures/White.png");
	}

	void OnUpdate(Fish::Timestep ts)override {
		// Update
		m_CameraController.OnUpdate(ts);

		// Render
		Fish::Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });

		Fish::Renderer::BeginScene(m_CameraController.GetCamera());

		const glm::vec4 color = glm::vec4(m_SquareColor, 1.0f);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		for (int y = 0; y < 20; y++)
		{
			for (int x = 0; x < 20; x++)
			{
				glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
				Fish::Renderer::Submit({
					.shader       = m_TextureShader,
					.vertexBuffer = m_SquareVB,
					.indexBuffer  = m_SquareIB,
					.texture      = m_WhiteTexture,//建立纹理库,这里换成索引
					.transform    = glm::translate(glm::mat4(1.0f), pos) * scale,
					.color        = color });
			}
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

private:
	Fish::Ref<Fish::Shader> m_FlatColorShader;   // 零调用者了,留着没删
	Fish::Ref<Fish::Shader> m_TextureShader;
	Fish::Ref<Fish::Texture2D> m_WhiteTexture;
	Fish::Ref<Fish::VertexBuffer> m_SquareVB;
	Fish::Ref<Fish::IndexBuffer>  m_SquareIB;
	Fish::Ref<Fish::VertexBuffer> m_TriangleVB;
	Fish::Ref<Fish::IndexBuffer>  m_TriangleIB;
	Fish::Ref<Fish::Texture2D> m_Texture, m_LogoTexture;

	Fish::OrthographicCameraController m_CameraController;

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
