#pragma once

#include "glm/glm.hpp"

#include <cstddef>
#include <cstdint>

namespace Fish {
	// 字段顺序就是内存顺序。shader 按 pos、uv、normal 声明,反射的前缀和才给出
	// 0/12/20;法线在这轮只存不读,阶段 5 再进 VSInput。
	struct MeshVertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
	};

	inline constexpr uint32_t k_MeshVertexStride = 32;

	static_assert(sizeof(MeshVertex) == k_MeshVertexStride, "MeshVertex 布局和预期不符");
	static_assert(offsetof(MeshVertex, position) == 0,  "position 不在 offset 0");
	static_assert(offsetof(MeshVertex, uv) == 12,       "uv 不在 offset 12");
	static_assert(offsetof(MeshVertex, normal) == 20,   "normal 不在 offset 20");

	using MeshId = uint32_t;
	inline constexpr MeshId k_InvalidMeshId = 0xFFFFFFFFu;

	// 一段几何在大 buffer 里的位置。meshId 就是这张表的下标,不在 Mesh 里再存一份。
	struct Mesh
	{
		uint32_t  firstVertex = 0;   // 单位:顶点
		uint32_t  vertexCount = 0;
		uint32_t  firstIndex  = 0;   // 单位:索引
		uint32_t  indexCount  = 0;   // 0 表示非索引绘制
		glm::vec3 aabbMin{};
		glm::vec3 aabbMax{};
	};
}
