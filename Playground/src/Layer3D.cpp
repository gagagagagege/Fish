#include "Layer3D.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {
	// 光和阴影相机必须同向。
	const glm::vec3 k_LightDirection = glm::normalize(glm::vec3(0.4f, -1.0f, -0.6f));
	const glm::vec3 k_ShadowTarget   = glm::vec3(0.0f, -1.5f, -5.0f);

	// 透视相机的默认位姿在 (0,0,3) 看向 -z,所以场景放在 -z 方向。
	const glm::vec3 k_GroundCenter = glm::vec3(0.0f, -2.0f, -6.0f);
}

Layer3D::Layer3D()
	: Layer("3dLayer"),
	  m_CameraController(1280.0f / 720.0f),
	  m_ShadowCamera(k_ShadowTarget - k_LightDirection * 12.0f, k_ShadowTarget,
	                 glm::vec3(0.0f, 1.0f, 0.0f), 8.0f, 0.1f, 40.0f)
{
	const std::string shaderDir = "Fish/src/Platform/Vulkan/shaders/";
	m_LitShader = Fish::Renderer::CreateShader("Lit", shaderDir + "lit.spv");

	m_WhiteTexture = Fish::Renderer::CreateTexture2D("Playground/assets/textures/White.png");

	// 1×1×1,中心在原点,y 从 -0.5 到 0.5。
	m_CubeScene = Fish::ImportScene("Playground/assets/models/cube.obj", m_LitShader);

	// 地面用同一份单位 quad,法线 +z,靠变换转到 XZ 平面。
	const Fish::MeshVertex groundVertices[4] = {
		{ {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
		{ { 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
		{ { 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
		{ {-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
	};
	const uint32_t groundIndices[6] = { 0, 1, 2, 2, 3, 0 };
	m_GroundMesh = Fish::Renderer::CreateMesh(groundVertices, 4, groundIndices, 6);

	// 相机只登记一次;控制器每帧改的是 Camera 对象本身。
	m_MainCameraId   = Fish::Renderer::AddCamera(m_CameraController.GetCamera());
	m_ShadowCameraId = Fish::Renderer::AddCamera(m_ShadowCamera);
	Fish::Renderer::SetPassCamera(Fish::RenderPass::Main, m_MainCameraId);
	Fish::Renderer::SetPassCamera(Fish::RenderPass::Shadow, m_ShadowCameraId);
	Fish::Renderer::SetLight({ .direction = k_LightDirection });
}

void Layer3D::OnUpdate(Fish::Timestep ts)
{
	m_CameraController.OnUpdate(ts);

	Fish::Renderer::SetClearColor({ 0.08f, 0.09f, 0.12f, 1.0f });

	// 地面:只接收阴影,不投影。local quad 绕 X 转 -90° 后落在 XZ 平面。
	const glm::mat4 groundTransform =
		glm::translate(glm::mat4(1.0f), k_GroundCenter)
		* glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
		* glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 20.0f, 1.0f));

	Fish::Renderer::Submit({
		.meshId      = m_GroundMesh,
		.transform   = groundTransform,
		.color       = m_GroundColor,
		.textureSlot = m_WhiteTexture->GetBindlessIndex(),
		.samplerSlot = m_WhiteTexture->GetSamplerIndex(),
		.castsShadow = false,
		.shader      = m_LitShader });

	struct CubePlacement
	{
		glm::vec3 position;
		float     scale;
		float     yawDegrees;
	};

	const CubePlacement placements[] = {
		{ {  0.0f, -1.5f, -4.0f }, 1.0f,  20.0f },
		{ {  2.2f, -1.5f, -6.0f }, 1.4f,  45.0f },
		{ { -2.0f, -1.5f, -5.0f }, 0.8f, -15.0f },
	};

	for (const CubePlacement& placement : placements) {
		const glm::mat4 transform =
			glm::translate(glm::mat4(1.0f), placement.position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(placement.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(placement.scale));

		for (const Fish::Instance& imported : m_CubeScene.instances) {
			Fish::Instance instance = imported;
			instance.transform   = transform * instance.transform;
			instance.color       = m_CubeColor;
			instance.textureSlot = m_WhiteTexture->GetBindlessIndex();
			instance.samplerSlot = m_WhiteTexture->GetSamplerIndex();
			Fish::Renderer::Submit(instance);
		}
	}

	Fish::Renderer::EndScene();
}

void Layer3D::OnEvent(Fish::Event& e)
{
	m_CameraController.OnEvent(e);
}
