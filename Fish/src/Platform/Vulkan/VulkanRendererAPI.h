#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include "Renderer/RendererAPI.h"
#include "VulkanValidationLayers.h"
#include "VulkanContext.h"

#include <memory>

namespace Fish {
	class VulkanRendererAPI : public RendererAPI
	{
	public:
		VulkanRendererAPI() = default;

		virtual void Init(void* nativeWindow) override;
		virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) override;

	public:
		vk::raii::Context                m_Context;
		vk::raii::Instance               m_Instance = nullptr;
		vk::raii::DebugUtilsMessengerEXT m_DebugMessenger = nullptr;
		vk::raii::SurfaceKHR             m_Surface = nullptr;

		std::unique_ptr<VulkanContext> m_DeviceContext;

	private:
		void createInstance();

		GLFWwindow* m_Window = nullptr;

		// 渲染路径还没接(W5 帧模型 / W6 管线),先存着
		glm::vec4   m_ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		vk::Rect2D  m_Viewport{};
	};
}
