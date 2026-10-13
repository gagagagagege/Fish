#pragma once
#include"glm/glm.hpp"

namespace Fish{
	class OrthographicCamera
	{
	public:
		// pos/neg 指世界坐标里的正向和负向,不是屏幕的上下左右。
		// y 那两个在实现里是反过来传给 glm::ortho 的 —— 见 .cpp 里的理由。
		OrthographicCamera(float pos_x, float neg_x, float pos_y, float neg_y);
		void SetProjection(float pos_x, float neg_x, float pos_y, float neg_y);

		const glm::vec3& GetPosition() const { return m_Position; }
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }

		float GetRotation() const { return m_Rotation; }
		void SetRotation(float rotation) { m_Rotation = rotation; RecalculateViewMatrix(); }

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
	private:
		void RecalculateViewMatrix();
	private:
		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ViewProjectionMatrix;

		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		float m_Rotation = 0.0f;
	};

}
