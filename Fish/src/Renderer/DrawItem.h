#pragma once

#include "Core/macro.h"
#include "glm/glm.hpp"

namespace Fish {
	class Shader;
	class VertexBuffer;
	class IndexBuffer;
	class Texture2D;

	// 一次绘制的全部输入。
	//
	// 后端无关:只有 Fish 的抽象类型和 glm,不含任何 vk:: 类型 ——
	// 这个头会被 Playground 间接包含,而那边没有 Vulkan 的编译定义。
	//
	// vertexBuffer 和 indexBuffer 是分开的两个字段,没有 VertexArray 夹在中间 ——
	// Vulkan 里顶点格式是逐管线的、绑定是逐命令的,没有"顶点数组"这个 GPU 对象。
	struct DrawItem
	{
		Ref<Shader>       shader;
		Ref<VertexBuffer> vertexBuffer;
		Ref<IndexBuffer>  indexBuffer;   // 可空:空走非索引绘制
		Ref<Texture2D>    texture;       // 可空:不采样的管线
		glm::mat4         transform = glm::mat4(1.0f);
		glm::vec4         color = glm::vec4(1.0f);   // 只有纯色管线读它

		// 由 Renderer::Submit 填,调用方不用管。
		//
		// 每个绘制项带自己的那份,而不是整帧共用一份 —— BeginScene 是每个 layer
		// 自己调的,两个 layer 各有相机,共用一个静态的话队列里所有项都会拿到
		// 最后那次 BeginScene 的值。
		glm::mat4         viewProjection = glm::mat4(1.0f);
	};
}
