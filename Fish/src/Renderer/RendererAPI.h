#pragma once
#include"glm/glm.hpp"
#include "VertexArray.h"
#include "Instance.h"
#include "DrawBatch.h"
#include "Light.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Fish {
	// 一帧里要录几条 pass。Main 是交换链那条,Shadow 是深度-only 那条。
	enum class RenderPass : uint32_t
	{
		Main   = 0,
		Shadow = 1,
		Count  = 2
	};

	inline constexpr size_t k_RenderPassCount = static_cast<size_t>(RenderPass::Count);

	// AddCamera 返回的下标,也是 cameraVPs 的下标。
	using CameraId = uint32_t;
	inline constexpr CameraId k_InvalidCameraId = ~0u;

	class RendererAPI
	{
	public:
		enum class API {
			None = 0, OpenGL = 1, Vulkan = 2
		};

		// 必须有,而且必须有 virtual。Renderer 存的是指向派生类的 RendererAPI*,
		// 非虚析构下 delete 只跑基类那一层 —— 2026-09-23 实测:去掉 virtual 后
		// 派生类析构不执行、整套 vk::raii 不销毁,而进程照样 exit=0、验证层 0 输出。
		virtual ~RendererAPI() = default;

		// 用 void* 而不是 GLFWwindow* 是为了不把 GLFW 头带进 Renderer/RendererAPI
		virtual void Init(void* nativeWindow) = 0;

		// 原来叫 setViewport 且收四个参数。Vulkan 侧 viewport 是录制时按交换链 extent 设的,
		// 一个参数都用不上,所以改成纯通知。
		virtual void NotifyWindowResized() {}

		virtual void SetClearColor(const glm::vec4& color) = 0;

		// 等 GPU 空转。销毁应用层持有的 GPU 资源之前必须调 —— 最后一帧的命令
		// 缓冲还引用着顶点缓冲和贴图,不等它们用完就销毁,验证层会报
		// VUID-vkDestroyBuffer-buffer-00922 / VUID-vkDestroySampler-sampler-01082。
		//
		// 放在 RendererAPI 上而不是让 Application 直接碰 device:那是后端细节,
		// Application 不该知道是 Vulkan。空实现给待删的 OpenGLRendererAPI 用。
		virtual void WaitIdle() {}

		// 一帧的全部:acquire -> 录 -> submit -> present。Clear() 没有单独的调用 ——
		// 动态渲染下清屏是 beginRendering 的 clearValue,用 SetClearColor 的值。
		//
		// items 是各 layer 这一帧 Submit 攒下的,每项自带 shader / 缓冲 / 贴图 /
		// 颜色 / 变换。相机不在 item 上:每个 pass 用 passCameras 查到的相机,
		// 所以同一份 items 会被录进多条 pass。
		//
		// cameraVPs 的下标是 CameraId;passCameras 是 pass → CameraId。
		// 都是每帧由 Renderer 从 Camera* 数组解析出来的值。
		//
		// light 是全局方向光,跨帧保留;它不是 Instance 的属性,所以走参数而不是塞进 items。
		//
		// 非纯虚是为了待删的 OpenGLRendererAPI 还能实例化。
		virtual void DrawFrame(const std::vector<Instance>& items, const Light& light,
			const std::vector<glm::mat4>& cameraVPs,
			const std::array<CameraId, k_RenderPassCount>& passCameras)
		{
			(void)items;
			(void)light;
			(void)cameraVPs;
			(void)passCameras;
		}

		// 批处理那条路的提交口。和 Submit 一样只入队 —— 攒进后端持有的某个批里,
		// 真正的绘制在 DrawFrame 的批处理段。非纯虚,理由同 DrawFrame:
		// 待删的 OpenGLRendererAPI 还得能实例化。
		virtual void SubmitBatch(const DrawBatch& batch)
		{
			(void)batch;
		}

		// 老的立即绘制接口。Submit 改成只入队之后引擎里没有调用者了,
		// 只剩 OpenGL 侧那个 override —— 等 Platform/OpenGL/ 整批下线时一起删。
		// 空实现而不是纯虚,理由同 Shader::Bind。
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) {}

		static const API GetAPI() { return s_API; }
	private:
		static API s_API;
	};
}


