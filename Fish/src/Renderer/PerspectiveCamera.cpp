#include"fspch.h"
#include"PerspectiveCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace {
	// Vulkan 的 NDC +y 落在图像底部(视口变换),屏幕相机要翻一次 y。
	const glm::mat4 k_FlipY = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
}

namespace Fish {
	PerspectiveCamera::PerspectiveCamera(const glm::vec3& position, float yaw, float pitch,
	                                     float fovy, float aspect, float zNear, float zFar)
	{
		m_Position = position;
		SetYawPitch(yaw, pitch);
		SetProjection(fovy, aspect, zNear, zFar);
	}

	PerspectiveCamera::PerspectiveCamera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up,
	                                     float fovy, float aspect, float zNear, float zFar)
		: m_Up(up)
	{
		m_Position = position;
		// target == position 时归一化得到 NaN,后面整个矩阵都是 NaN,不会报错。
		m_Forward = glm::normalize(target - position);
		RecalculateViewMatrix();
		SetProjection(fovy, aspect, zNear, zFar);
	}

	void PerspectiveCamera::SetPosition(const glm::vec3& position)
	{
		m_Position = position;
		RecalculateViewMatrix();
	}

	void PerspectiveCamera::SetYawPitch(float yaw, float pitch)
	{
		const float yawRad   = glm::radians(yaw);
		const float pitchRad = glm::radians(pitch);
		m_Forward = glm::vec3(std::cos(pitchRad) * std::sin(yawRad),
		                      std::sin(pitchRad),
		                      -std::cos(pitchRad) * std::cos(yawRad));
		m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
		RecalculateViewMatrix();
	}

	void PerspectiveCamera::SetProjection(float fovy, float aspect, float zNear, float zFar)
	{
		m_ProjectionMatrix = glm::perspective(glm::radians(fovy), aspect, zNear, zFar);
		m_ViewProjectionMatrix = k_FlipY * m_ProjectionMatrix * m_ViewMatrix;
	}

	void PerspectiveCamera::RecalculateViewMatrix()
	{
		m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
		m_ViewProjectionMatrix = k_FlipY * m_ProjectionMatrix * m_ViewMatrix;
	}
}
