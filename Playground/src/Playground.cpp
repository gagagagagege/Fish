#include"imgui.h"
#include"includings.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Platform/OpenGL/OpenGLShader.h"

#include"playground2D.h"
#include"Core/EntryPoint.h"

class ExampleLayer :public Fish::Layer {
public:
	ExampleLayer() :Layer("Example") , m_CameraController(1280.0f / 720.0f) {
		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;

            uniform mat4 u_ViewProjection;
	        uniform mat4 u_Transform;

			out vec3 v_Position;
			out vec4 v_Color;

			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);	
			}
		)";

		std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;

			in vec3 v_Position;
			in vec4 v_Color;

			void main()
			{
				color = v_Color;
			}
		)";
		m_Shader = Fish::Shader::Create("VertexPosColor", vertexSrc, fragmentSrc);

		m_VertexArray.reset(Fish::VertexArray::Create());

		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
			 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
			 0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f
		};

		Fish::Ref<Fish::VertexBuffer> vertexBuffer;
		vertexBuffer.reset(Fish::VertexBuffer::Create(vertices, sizeof(vertices)));
		Fish::BufferLayout layout = {
			{ Fish::ShaderDataType::Float3, "a_Position" },
			{ Fish::ShaderDataType::Float4, "a_Color" }
		};
		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);

		uint32_t indices[3] = { 0, 1, 2 };
		Fish::Ref<Fish::IndexBuffer> indexBuffer;
		indexBuffer.reset(Fish::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		std::string flatColorShaderVertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;

            uniform mat4 u_ViewProjection;
            uniform mat4 u_Transform;


			out vec3 v_Position;

			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);	
			}
		)";
 
		std::string flatColorShaderFragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;

			in vec3 v_Position;
            uniform vec3 u_Color;

			void main()
			{
				color = vec4(u_Color, 1.0);
			}
		)";
		m_FlatColorShader = Fish::Shader::Create("FlatColor", flatColorShaderVertexSrc, flatColorShaderFragmentSrc);
		std::dynamic_pointer_cast<Fish::OpenGLShader>(m_FlatColorShader)->Bind();
		std::dynamic_pointer_cast<Fish::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat3("u_Color", m_SquareColor);

		auto textureShader = m_ShaderLibrary.Load("Playground/assets/shaders/Texture.glsl");
		m_Texture = Fish::Texture2D::Create("Playground/assets/textures/Checkerboard.png");
		m_LogoTexture = Fish::Texture2D::Create("Playground/assets/textures/ChernoLogo.png");
		std::dynamic_pointer_cast<Fish::OpenGLShader>(textureShader)->Bind();
		std::dynamic_pointer_cast<Fish::OpenGLShader>(textureShader)->UploadUniformInt("u_Texture", 0);

		float squareVertices[5 * 4] = {
	        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
		};


		m_SquareVA.reset(Fish::VertexArray::Create());
		Fish::Ref<Fish::VertexBuffer> squareVB;
		squareVB.reset(Fish::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
		squareVB->SetLayout({
			{ Fish::ShaderDataType::Float3, "a_Position" },
			{ Fish::ShaderDataType::Float2, "a_TexCoord"}
			});
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
		Fish::Ref<Fish::IndexBuffer> squareIB;
		squareIB.reset(Fish::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));
		m_SquareVA->SetIndexBuffer(squareIB);
	}

	void OnUpdate(Fish::Timestep ts)override {
		// Update
		m_CameraController.OnUpdate(ts);

		// Render
		Fish::Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Fish::Renderer::Clear();

		Fish::Renderer::BeginScene(m_CameraController.GetCamera());
		// 1
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		for (int y = 0; y < 20; y++)
		{
			for (int x = 0; x < 20; x++)
			{
				glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				Fish::Renderer::Submit(m_FlatColorShader, m_SquareVA, transform);
			}
		}
		// 2
		auto textureShader = m_ShaderLibrary.Get("Texture");
		m_Texture->Bind();
		Fish::Renderer::Submit(textureShader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
		m_LogoTexture->Bind();
		Fish::Renderer::Submit(textureShader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
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
	Fish::ShaderLibrary m_ShaderLibrary;
	Fish::Ref<Fish::Shader> m_Shader;
	Fish::Ref<Fish::Shader> m_FlatColorShader;
	Fish::Ref<Fish::VertexArray> m_SquareVA;
	Fish::Ref<Fish::VertexArray> m_VertexArray;
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