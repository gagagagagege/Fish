#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include "Renderer/RendererAPI.h"
#include "VulkanValidationLayers.h"
#include "VulkanContext.h"
#include "VulkanSwapChain.h"
#include "VulkanCommandPool.h"
#include "VulkanDescriptorAllocator.h"
#include "VulkanFrameData.h"
#include "VulkanPipelineCache.h"
#include "VulkanTexture.h"
#include "VulkanTextureDescriptorCache.h"

#include <memory>
#include <optional>

namespace Fish {
	class VulkanRendererAPI : public RendererAPI
	{
	public:
		VulkanRendererAPI() = default;
		~VulkanRendererAPI();

		virtual void Init(void* nativeWindow) override;
		virtual void NotifyWindowResized() override;
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void WaitIdle() override;
		virtual void DrawFrame(const std::vector<DrawItem>& items) override;

	public:
		vk::raii::Context                m_Context;
		vk::raii::Instance               m_Instance = nullptr;
		vk::raii::DebugUtilsMessengerEXT m_DebugMessenger = nullptr;
		vk::raii::SurfaceKHR             m_Surface = nullptr;

		std::unique_ptr<VulkanContext> m_DeviceContext;

		SwapChain           m_SwapChain;
		DescriptorAllocator m_DescriptorAllocator;
		// 后两个都只能等 shader 创建出来才存在(shader 是应用层的 layer 建的,
		// 比 Init 晚),所以推迟到第一次 DrawFrame 才建。见 EnsureRenderState。
		std::optional<PipelineCache>          m_PipelineCache;
		std::optional<TextureDescriptorCache> m_TextureSets;
		CommandPool         m_CommandPool;
		CommandPool         m_TransientPool;
		Frames              m_Frames;

	private:
		void createInstance();
		void EnsureRenderState();

		GLFWwindow* m_Window = nullptr;

		// DrawFrame 开头消费它。重建放在 acquire 之前,否则会把刚拿到的图像丢掉。
		bool m_FramebufferResized = false;

		glm::vec4 m_ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	};
}
