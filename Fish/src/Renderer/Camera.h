#pragma once
#include "glm/glm.hpp"

namespace Fish {
	// 渲染器只读 viewProjection;投影和 view 怎么来的是子类的事。
	// y 取反不在这里 —— 它是"画到交换链"的约定,屏幕相机各自乘,阴影相机不乘。
	class Camera
	{
	public:
		virtual ~Camera() = default;

		const glm::vec3& GetPosition() const { return m_Position; }
		virtual void SetPosition(const glm::vec3& position) = 0;

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

	protected:
		glm::mat4 m_ProjectionMatrix{ 1.0f };
		glm::mat4 m_ViewMatrix{ 1.0f };
		glm::mat4 m_ViewProjectionMatrix{ 1.0f };
		glm::vec3 m_Position{ 0.0f };
	};
}
