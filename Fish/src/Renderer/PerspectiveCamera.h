#pragma once
#include"glm/glm.hpp"
#include"Camera.h"

namespace Fish{
	class PerspectiveCamera : public Camera
	{
	public:
		// 角度单位是度,和 OrthographicCamera::SetRotation 一致。
		// yaw 绕世界 +y,0 度看向 -z;pitch 是俯仰。up 固定为世界 +y。
		PerspectiveCamera(const glm::vec3& position, float yaw, float pitch,
		                  float fovy, float aspect, float zNear, float zFar);
		// 看向 target。方向与 up 平行时 glm::lookAt 退化。
		PerspectiveCamera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up,
		                  float fovy, float aspect, float zNear, float zFar);

		void SetPosition(const glm::vec3& position) override;
		// 会覆盖目标相机那个构造里传的 up。控制器每帧调它。
		void SetYawPitch(float yaw, float pitch);
		void SetProjection(float fovy, float aspect, float zNear, float zFar);

		// 控制器要用它算前后左右的移动方向。
		const glm::vec3& GetForward() const { return m_Forward; }

	private:
		void RecalculateViewMatrix();

		glm::vec3 m_Forward{ 0.0f, 0.0f, -1.0f };
		glm::vec3 m_Up{ 0.0f, 1.0f, 0.0f };
	};

}
