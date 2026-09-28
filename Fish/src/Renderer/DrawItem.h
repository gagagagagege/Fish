#pragma once

#include "Core/macro.h"
#include "glm/glm.hpp"

namespace Fish {
	class Shader;
	class VertexBuffer;
	class IndexBuffer;
	class Texture2D;

	struct DrawItem
	{
		Ref<Shader>       shader;
		Ref<VertexBuffer> vertexBuffer;
		Ref<IndexBuffer>  indexBuffer;   // 可空
		Ref<Texture2D>    texture;       // 可空
		glm::mat4         transform = glm::mat4(1.0f);
		glm::vec4         color = glm::vec4(1.0f);   // 只有纯色管线读

		glm::mat4         viewProjection = glm::mat4(1.0f);
	};
}
