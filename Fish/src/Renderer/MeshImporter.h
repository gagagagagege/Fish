#pragma once

#include "Renderer/Instance.h"

#include <string>

namespace Fish {
	class Shader;

	// 导入一个静态场景:每个 aiMesh 一个 Mesh,每个节点里的每个 mesh 一个 Instance。
	// 蒙皮 mesh 跳过;材质/贴图不导入,textureSlot 留 0。
	// 失败时返回空 SceneInstance,原因打到 FS_CORE_ERROR。
	SceneInstance ImportScene(const std::string& path, const Ref<Shader>& shader);
}
