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
#include <cstdint>
#include <deque>
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

		struct BindlessTextureIndices
		{
			uint32_t image   = k_InvalidBindlessIndex;
			uint32_t sampler = k_InvalidBindlessIndex;
		};

		// 往图像表和采样器表里各加一格,返回两个槽位。槽位由这里发号,应用层不管。
		BindlessTextureIndices RegisterTexture(texture& tex);

		// 贴图析构时调。GPU 对象一起交进来,等 k_MaxFramesInFlight 帧再销毁 ——
		// 只还槽位号的话,image/view/sampler 会在在飞的命令缓冲还在采样它的时候被销毁。
		void ReleaseTexture(texture&& gpu, uint32_t imageSlot, uint32_t samplerSlot);

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

		struct DeferredTexture
		{
			texture  gpu;
			uint32_t imageSlot;
			uint32_t samplerSlot;
			uint64_t frame;      // 交进来时的帧号
		};

		void EnsureBatchBuffer(BatchBuffer& target, uint32_t needed, uint32_t initial);
		void EnsureTextureTable();
		void RecordBatches(vk::raii::CommandBuffer& commandBuffer);

		// 帧首、等过当前帧槽的 fence 之后调 —— 那个等待才是"安全"的来源。
		void     ReclaimTextures();
		uint32_t TakeImageSlot();
		uint32_t TakeSamplerSlot();

		std::array<BatchState, k_BatchShapeCount> m_Batches;

		// 两张表。set 建出来之前只攒着,建出来之后每加一张立刻写对应的一格 ——
		// updateAfterBind 让"表已经录进在飞命令缓冲之后再写它"合法。
		vk::raii::DescriptorSet              m_TextureTableSet = nullptr;
		std::vector<vk::DescriptorImageInfo> m_ImageInfos;     // 下标就是图像槽位
		std::vector<vk::DescriptorImageInfo> m_SamplerInfos;   // 下标就是采样器槽位
		uint32_t m_NextImageSlot   = 0;
		uint32_t m_NextSamplerSlot = 0;
		uint32_t m_ImageCapacity   = 0;
		uint32_t m_SamplerCapacity = 0;

		// 延迟销毁队列。按帧号递增入队,所以队头就是最早交进来的。
		std::deque<DeferredTexture> m_DeferredTextures;
		std::vector<uint32_t>       m_FreeImageSlots;
		std::vector<uint32_t>       m_FreeSamplerSlots;

		// 已开始的帧数。只在等过当前帧槽的 fence 之后自增。
		uint64_t m_FrameNumber = 0;

		GLFWwindow* m_Window = nullptr;

		bool m_FramebufferResized = false;

		glm::vec4 m_ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	};
}
