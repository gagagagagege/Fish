// 照搬 Unreal Engine 的编辑器飞行相机,出处见 .h 开头。
#include"fspch.h"
#include"PerspectiveCameraController.h"

#include"Core/Application.h"
#include"Core/Input.h"
#include"Core/KeyCodes.h"
#include"Core/MouseCodes.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr float k_MoveSpeed        = 5.0f;    // 世界单位/秒
	constexpr float k_MouseSensitivity = 0.15f;  // 度/像素
	constexpr float k_FovSpeed         = 60.0f;  // 度/秒
	constexpr float k_MinFov           = 10.0f;
	constexpr float k_MaxFov           = 120.0f;
	constexpr float k_MaxPitch         = 89.0f;
	// UE 在进模拟前也会夹一次 dt(EditorMovementDeltaUpperBound),避免卡顿后镜头飞出去。
	constexpr float k_MaxDeltaTime     = 0.1f;
}

namespace Fish {
	PerspectiveCameraController::PerspectiveCameraController(float aspectRatio, float fovy,
	                                                         float zNear, float zFar)
		: m_AspectRatio(aspectRatio), m_Fov(fovy), m_Near(zNear), m_Far(zFar),
		  m_Camera(m_Position, 0.0f, 0.0f, fovy, aspectRatio, zNear, zFar)
	{}

	void PerspectiveCameraController::OnUpdate(Timestep ts)
	{
		const float dt = std::min(ts.GetSeconds(), k_MaxDeltaTime);
		Window& window = Application::Get().GetWindow();

		const bool looking = Input::IsMouseButtonPressed(Mouse::ButtonRight);
		if (looking != m_Looking) {
			m_Looking = looking;
			window.SetCursorLocked(looking);
			// 锁上那一刻光标会跳,先记下当前位置,这一帧不产生位移。
			const auto [x, y] = Input::GetMousePosition();
			m_LastMouse = { x, y };
		}

		if (!m_Looking)
			return;

		const auto [mouseX, mouseY] = Input::GetMousePosition();
		const float dx = mouseX - m_LastMouse.first;
		const float dy = mouseY - m_LastMouse.second;
		m_LastMouse = { mouseX, mouseY };

		// 鼠标看是"每像素多少度",不乘 dt —— UE 的 RotateVelocityModifier 就是这条。
		m_Yaw   += dx * k_MouseSensitivity;
		m_Pitch -= dy * k_MouseSensitivity;
		m_Pitch  = std::clamp(m_Pitch, -k_MaxPitch, k_MaxPitch);
		m_Camera.SetYawPitch(m_Yaw, m_Pitch);

		// 前后左右用相机局部空间,上下用世界轴。
		float forwardInput = 0.0f, rightInput = 0.0f, upInput = 0.0f;
		if (Input::IsKeyPressed(Key::W))            forwardInput += 1.0f;
		if (Input::IsKeyPressed(Key::S))            forwardInput -= 1.0f;
		if (Input::IsKeyPressed(Key::D))            rightInput += 1.0f;
		if (Input::IsKeyPressed(Key::A))            rightInput -= 1.0f;
		if (Input::IsKeyPressed(Key::Space))        upInput += 1.0f;
		if (Input::IsKeyPressed(Key::LeftControl))  upInput -= 1.0f;

		const glm::vec3 forward = m_Camera.GetForward();
		const glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 move = forward * forwardInput + right * rightInput + glm::vec3(0.0f, upInput, 0.0f);

		// 非物理分支:速度 = 输入方向 × 速度。规整一下,斜着走不会更快。
		if (glm::dot(move, move) > 1.0f)
			move = glm::normalize(move);

		m_Position += move * k_MoveSpeed * dt;
		m_Camera.SetPosition(m_Position);

		// UE 的 FOV 缩放是 C / Z,这里照搬。
		float fov = m_Fov;
		if (Input::IsKeyPressed(Key::C)) fov -= k_FovSpeed * dt;
		if (Input::IsKeyPressed(Key::Z)) fov += k_FovSpeed * dt;
		fov = std::clamp(fov, k_MinFov, k_MaxFov);
		if (fov != m_Fov) {
			m_Fov = fov;
			m_Camera.SetProjection(m_Fov, m_AspectRatio, m_Near, m_Far);
		}
	}

	void PerspectiveCameraController::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(FS_BIND_EVENT_FN(PerspectiveCameraController::OnWindowResized));
	}

	bool PerspectiveCameraController::OnWindowResized(WindowResizeEvent& e)
	{
		m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
		m_Camera.SetProjection(m_Fov, m_AspectRatio, m_Near, m_Far);
		return false;
	}
}
