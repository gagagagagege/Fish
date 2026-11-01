#pragma once

#include "Core/macro.h"
#include "Renderer/Mesh.h"
#include "glm/glm.hpp"

#include <vector>

namespace Fish {
	class Shader;

	// 场景里的一个实例。几何由 meshId 指向 mesh 表,这里只放逐实例的数据。
	// shader 这轮表示主 pass 的材质 shader;多 pass 时管线选择要移到 pass 层。
	struct Instance
	{
		MeshId      meshId      = k_InvalidMeshId;
		glm::mat4   transform   = glm::mat4(1.0f);
		glm::vec4   color       = glm::vec4(1.0f);
		uint32_t    textureSlot = 0;   // 图像表槽位
		uint32_t    samplerSlot = 0;   // 采样器表槽位
		Ref<Shader> shader;
	};

	struct SceneInstance
	{
		std::vector<Instance> instances;
	};

	// Submit 每帧入队的一项:Instance 本体 + 按相机查出来的 VP。
	struct RenderInstance
	{
		Instance  instance;
		glm::mat4 viewProjection = glm::mat4(1.0f);
	};
}
