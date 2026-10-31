// 照搬 Unreal Engine 的编辑器飞行相机。
//   Engine/Source/Editor/UnrealEd/Private/EditorViewportClient.cpp
//     FEditorViewportClient::UpdateCameraMovement —— 按键只产生 -1/0/+1 的轴 impulse
//   Engine/Source/Editor/UnrealEd/Private/CameraController.cpp
//     FEditorCameraController::UpdatePosition / UpdateRotation 的"不用物理"分支
//
// 和 UE 不同的地方:上下是 Space / Ctrl(UE 是 E / Q);速度写死,没有档位和滚轮;
// 前后左右在相机局部空间、上下用世界轴 —— 这一条和 UE 一致。
#pragma once
#include"Renderer/PerspectiveCamera.h"
#include"Core/Timestep.h"
#include"Event/ApplicationEvent.h"
#include"Event/MouseEvent.h"

#include <utility>

namespace Fish {
	class PerspectiveCameraController
	{
	public:
		PerspectiveCameraController(float aspectRatio, float fovy = 60.0f,
		                            float zNear = 0.1f, float zFar = 1000.0f);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		PerspectiveCamera& GetCamera() { return m_Camera; }
		const PerspectiveCamera& GetCamera() const { return m_Camera; }

	private:
		bool OnWindowResized(WindowResizeEvent& e);

	private:
		float m_AspectRatio;
		float m_Fov;
		float m_Near;
		float m_Far;

		PerspectiveCamera m_Camera;

		glm::vec3 m_Position{ 0.0f, 0.0f, 3.0f };
		float m_Yaw = 0.0f;      // 度
		float m_Pitch = 0.0f;

		// 鼠标看只在按住右键时生效;按住时锁光标。
		bool m_Looking = false;
		std::pair<float, float> m_LastMouse{ 0.0f, 0.0f };
	};
}
