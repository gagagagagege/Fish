#pragma once

#include "includings.h"
#include "Renderer/MeshImporter.h"
#include "Renderer/ShadowCamera.h"
#include "Core/PerspectiveCameraController.h"

// 类名不能以数字开头,所以 C++ 里叫 Layer3D;Layer 的显示名是 "3dLayer"。
class Layer3D : public Fish::Layer
{
public:
	Layer3D();
	~Layer3D() override = default;

	void OnAttach() override {}
	void OnDetach() override {}
	void OnUpdate(Fish::Timestep ts) override;
	void OnEvent(Fish::Event& e) override;

private:
	Fish::Ref<Fish::Shader>    m_LitShader;
	Fish::Ref<Fish::Texture2D> m_WhiteTexture;
	Fish::SceneInstance        m_CubeScene;
	Fish::MeshId               m_GroundMesh = Fish::k_InvalidMeshId;

	Fish::PerspectiveCameraController m_CameraController;
	Fish::ShadowCamera                m_ShadowCamera;
	Fish::CameraId                    m_MainCameraId   = Fish::k_InvalidCameraId;
	Fish::CameraId                    m_ShadowCameraId = Fish::k_InvalidCameraId;

	glm::vec4 m_GroundColor = { 0.35f, 0.35f, 0.35f, 1.0f };
	glm::vec4 m_CubeColor   = { 0.90f, 0.50f, 0.20f, 1.0f };
};
