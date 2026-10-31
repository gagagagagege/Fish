#pragma once
#include"glm/glm.hpp"
#include"Camera.h"

namespace Fish{
	// 平行光的阴影相机:从 position 看向 target,正方形正交视锥。
	// 不乘 k_FlipY —— 它渲染到深度图,不经过屏幕。
	class ShadowCamera : public Camera
	{
	public:
		// halfSize 是视锥的半宽/半高;zNear / zFar 是沿光线方向的距离。
		ShadowCamera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up,
		             float halfSize, float zNear, float zFar);

		void SetPosition(const glm::vec3& position) override;
		void SetProjection(float halfSize, float zNear, float zFar);

	private:
		void RecalculateViewMatrix();

		glm::vec3 m_Forward{ 0.0f, 0.0f, -1.0f };
		glm::vec3 m_Up{ 0.0f, 1.0f, 0.0f };
	};

}
