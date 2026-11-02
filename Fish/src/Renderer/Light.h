#pragma once

#include "glm/glm.hpp"

namespace Fish {
	// 一个方向光。三个量都是世界空间、线性颜色。
	// direction 是光的传播方向(从上往下照是 (0,-1,0)),shader 里取反得到指向光源的 L。
	struct Light
	{
		glm::vec3 direction = glm::vec3(-0.57735f);
		glm::vec3 color     = glm::vec3(1.0f);   // 强度已经乘进来
		glm::vec3 ambient   = glm::vec3(0.10f);
	};
}
