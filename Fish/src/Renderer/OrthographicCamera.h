#pragma once
#include"glm/glm.hpp"
#include"Camera.h"

namespace Fish{
	class OrthographicCamera : public Camera
	{
	public:
		// pos/neg 指世界坐标里的正向和负向,不是屏幕的上下左右。
		// 投影和 view 都是标准 GL 约定;翻 y 在 Camera 里乘进 viewProjection。
		OrthographicCamera(float pos_x, float neg_x, float pos_y, float neg_y);
		void SetProjection(float pos_x, float neg_x, float pos_y, float neg_y);

		void SetPosition(const glm::vec3& position) override { m_Position = position; RecalculateViewMatrix(); }

		float GetRotation() const { return m_Rotation; }
		void SetRotation(float rotation) { m_Rotation = rotation; RecalculateViewMatrix(); }
	private:
		void RecalculateViewMatrix();
	private:
		float m_Rotation = 0.0f;
	};

}
