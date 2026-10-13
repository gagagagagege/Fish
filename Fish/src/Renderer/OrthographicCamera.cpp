#include "D:/Fish/build/CMakeFiles/Fish.dir/Debug/cmake_pch.hxx"
#include "OrthographicCamera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Fish {
	// glm::ortho 产出的是 GL 约定的 NDC(y 向上),Vulkan 的 NDC y 向下。
	// 所以 y 两个参数反过来传 —— 不反的话整个画面上下镜像:物体位置上下颠倒,
	// 相机按 W 往 +y 走,画面里的东西却往上跑。
	OrthographicCamera::OrthographicCamera(float pos_x, float neg_x, float pos_y, float neg_y)
		: m_ProjectionMatrix(glm::ortho(neg_x, pos_x, pos_y, neg_y, -1.0f, 1.0f)), m_ViewMatrix(1.0f)
	{
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
	void OrthographicCamera::SetProjection(float pos_x, float neg_x, float pos_y, float neg_y)
	{
		m_ProjectionMatrix = glm::ortho(neg_x, pos_x, pos_y, neg_y, -1.0f, 1.0f);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}

	void OrthographicCamera::RecalculateViewMatrix()
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));

		m_ViewMatrix = glm::inverse(transform);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
}