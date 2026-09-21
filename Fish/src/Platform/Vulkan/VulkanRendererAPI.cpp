#include "VulkanRendererAPI.h"

#include "VulkanSurface.h"

#include <stdexcept>
#include <cstring>

namespace Fish {

	namespace {
		const std::vector<const char*> k_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
		const std::vector<const char*> k_DeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
		};
	}

	void VulkanRendererAPI::Init(void* nativeWindow)
	{
		m_Window = static_cast<GLFWwindow*>(nativeWindow);
		if (m_Window == nullptr) {
			throw std::runtime_error("VulkanRendererAPI::Init: nativeWindow is null");
		}

		createInstance();
		setupDebugMessenger(m_Instance, m_DebugMessenger);
		surface::createWin32Surface(m_Instance, m_Surface, m_Window);

		m_DeviceContext = std::make_unique<VulkanContext>(m_Instance, m_Surface, k_DeviceExtensions, k_ValidationLayers);
	}

	void VulkanRendererAPI::createInstance()
	{
		if (enableValidationLayers && !checkValidationLayerSupport(m_Context, k_ValidationLayers)) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		vk::ApplicationInfo appInfo{};
		appInfo.pApplicationName = "Fish";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Fish";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		auto requiredExtensions = getRequiredExtensions();

		// vkCreateInstance 自己会因为缺扩展失败,但只给 VK_ERROR_EXTENSION_NOT_PRESENT,
		// 看不出缺的是哪个。先查一遍把名字带进异常。
		auto availableExtensions = m_Context.enumerateInstanceExtensionProperties();
		for (const char* required : requiredExtensions) {
			const bool found = std::ranges::any_of(availableExtensions, [required](const auto& e) {
				return strcmp(required, e.extensionName) == 0;
			});
			if (!found) {
				throw std::runtime_error(std::string("required instance extension not supported: ") + required);
			}
		}

		vk::InstanceCreateInfo createInfo{};
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(k_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = k_ValidationLayers.data();
		}

		m_Instance = vk::raii::Instance(m_Context, createInfo);
	}

	void VulkanRendererAPI::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		m_Viewport = vk::Rect2D{ { static_cast<int32_t>(x), static_cast<int32_t>(y) },
								 { width, height } };
	}

	void VulkanRendererAPI::SetClearColor(const glm::vec4& color)
	{
		m_ClearColor = color;
	}

	// Clear / DrawIndexed 要有命令缓冲和帧同步才能做 —— 那是 W5 的帧模型和 W6 的管线。
	// 抛异常而不是空实现:空实现会让"跑起来了"和"什么都没画"看起来一样。
	void VulkanRendererAPI::Clear()
	{
		throw std::runtime_error("VulkanRendererAPI::Clear not implemented yet (needs the W5 frame model)");
	}

	void VulkanRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray)
	{
		throw std::runtime_error("VulkanRendererAPI::DrawIndexed not implemented yet (needs the W6 pipeline)");
	}
}
