#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include "Renderer/RendererAPI.h"
#include "Renderer/DrawBatch.h"
#include "VulkanValidationLayers.h"
#include "VulkanContext.h"
#include "VulkanSwapChain.h"
#include "VulkanCommandPool.h"
#include "VulkanDescriptorAllocator.h"
#include "VulkanFrameData.h"
#include "VulkanPipelineCache.h"
#include "VulkanTexture.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

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
		virtual void SubmitBatch(const DrawBatch& batch) override;

		// 往纹理表里加一张贴图,返回它的槽位。槽位由这里发号,应用层不管。
		uint32_t RegisterTexture(texture& tex);

	public:
		vk::raii::Context                m_Context;
		vk::raii::Instance               m_Instance = nullptr;
		vk::raii::DebugUtilsMessengerEXT m_DebugMessenger = nullptr;
		vk::raii::SurfaceKHR             m_Surface = nullptr;

		std::unique_ptr<VulkanContext> m_DeviceContext;

		SwapChain           m_SwapChain;
		DescriptorAllocator m_DescriptorAllocator;
		std::optional<PipelineCache>          m_PipelineCache;
		CommandPool         m_CommandPool;
		CommandPool         m_TransientPool;
		Frames              m_Frames;

	private:
		struct BatchBuffer
		{
			Buffer   buffer;
			void*    mapped = nullptr;
			uint32_t capacity = 0;      // 能装几个顶点
		};

		struct BatchState
		{
			std::vector<BatchVertex> vertices;                     
			std::vector<BatchBuffer> frames;                      
			Ref<Shader>              shader;                       
			glm::mat4                viewProjection = glm::mat4(1.0f);
		};

		void createInstance();
		void EnsureRenderState();

		void EnsureBatchBuffer(BatchBuffer& target, uint32_t needed, uint32_t initial);
		void EnsureTextureTable();
		void RecordBatches(vk::raii::CommandBuffer& commandBuffer);

		std::array<BatchState, k_BatchShapeCount> m_Batches;

		// 纹理表。set 建出来之前只攒着,建出来之后每加一张立刻写那一格 ——
		// updateAfterBind 让"表已经录进在飞命令缓冲之后再写它"合法。
		vk::raii::DescriptorSet              m_TextureTableSet = nullptr;
		std::vector<vk::DescriptorImageInfo> m_TextureInfos;   // 下标就是槽位
		uint32_t                             m_NextTextureSlot = 0;

		GLFWwindow* m_Window = nullptr;

		bool m_FramebufferResized = false;

		glm::vec4 m_ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	};
}
