#include "D:/Fish/build/CMakeFiles/Fish.dir/Debug/cmake_pch.hxx"
#include "OrthographicCamera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace {
	// Vulkan 的 NDC +y 落在图像底部(视口变换),屏幕相机要翻一次 y。
	const glm::mat4 k_FlipY = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
}

namespace Fish {
	OrthographicCamera::OrthographicCamera(float pos_x, float neg_x, float pos_y, float neg_y)
	{
		m_ProjectionMatrix = glm::ortho(neg_x, pos_x, neg_y, pos_y, -1.0f, 1.0f);
		m_ViewMatrix = glm::mat4(1.0f);
		m_ViewProjectionMatrix = k_FlipY * m_ProjectionMatrix * m_ViewMatrix;
	}
	void OrthographicCamera::SetProjection(float pos_x, float neg_x, float pos_y, float neg_y)
	{
		m_ProjectionMatrix = glm::ortho(neg_x, pos_x, neg_y, pos_y, -1.0f, 1.0f);
		m_ViewProjectionMatrix = k_FlipY * m_ProjectionMatrix * m_ViewMatrix;
	}

	void OrthographicCamera::RecalculateViewMatrix()
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));

		m_ViewMatrix = glm::inverse(transform);
		m_ViewProjectionMatrix = k_FlipY * m_ProjectionMatrix * m_ViewMatrix;
	}
}
