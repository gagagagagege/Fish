#include"fspch.h"
#include"ShadowCamera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Fish {
	ShadowCamera::ShadowCamera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up,
	                           float halfSize, float zNear, float zFar)
		: m_Up(up)
	{
		m_Position = position;
		// target == position 时归一化得到 NaN,后面整个矩阵都是 NaN,不会报错。
		m_Forward = glm::normalize(target - position);
		RecalculateViewMatrix();
		SetProjection(halfSize, zNear, zFar);
	}

	void ShadowCamera::SetPosition(const glm::vec3& position)
	{
		m_Position = position;
		RecalculateViewMatrix();
	}

	// 视锥按场景拟合是阴影 pass 那一轮的事,这里只收一个半宽。
	void ShadowCamera::SetProjection(float halfSize, float zNear, float zFar)
	{
		m_ProjectionMatrix = glm::ortho(-halfSize, halfSize, -halfSize, halfSize, zNear, zFar);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}

	void ShadowCamera::RecalculateViewMatrix()
	{
		m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
}
