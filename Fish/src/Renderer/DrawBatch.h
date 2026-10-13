#pragma once

#include "Core/macro.h"
#include "glm/glm.hpp"

#include <cstddef>
#include <cstdint>

namespace Fish {
	class Shader;
	enum class BatchShape : uint32_t
	{
		Triangle = 0,
		Quad     = 1
	};

	inline constexpr uint32_t k_BatchShapeCount = 2;

	inline constexpr uint32_t k_NoBatchTextureSlot = ~0u;

	// 字段顺序不能改:offset in shader
	struct BatchVertex
	{
		glm::vec3 position;
		glm::vec4 color;
		glm::vec2 uv;
		uint32_t  texId;
		float     tilingFactor;
	};

	inline constexpr uint32_t k_BatchVertexStride = 44;

	// glm 的 aligned 宏一旦打开,vec4 的对齐会把后面所有字段推走,而反射的前缀和
	// 是按紧凑排布算的 —— 两边错开,而 BuildVertexInput 只检查总和不超 stride,
	// 错开是查不出来的。这四条 static_assert 是唯一的哨兵。
	static_assert(sizeof(BatchVertex) == k_BatchVertexStride, "BatchVertex 布局和 batch.slang 对不上");
	static_assert(offsetof(BatchVertex, position) == 0,  "position 不在 offset 0");
	static_assert(offsetof(BatchVertex, color) == 12,    "color 不在 offset 12");
	static_assert(offsetof(BatchVertex, uv) == 28,       "uv 不在 offset 28");
	static_assert(offsetof(BatchVertex, texId) == 36,    "texId 不在 offset 36");
	static_assert(offsetof(BatchVertex, tilingFactor) == 40, "tilingFactor 不在 offset 40");

	//批的key里面没有viewProjection,两个layer viewProjection不同时会报
	struct DrawBatch
	{
		BatchShape  shape;
		Ref<Shader> shader;
		glm::mat4   transform = glm::mat4(1.0f);
		glm::vec4   color = glm::vec4(1.0f);
		uint32_t    texId = 0;
		float       tilingFactor = 1.0f;
		glm::mat4   viewProjection = glm::mat4(1.0f);
	};
}
