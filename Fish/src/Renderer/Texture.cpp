#include"fspch.h"

#include"Texture.h"
#include"Renderer.h"

#include "Platform/OpenGL/OpenGLTexture.h"

namespace Fish {
	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    FS_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLTexture2D>(path);
		//case RendererAPI::API::Vulkan:  return 
		}

		FS_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}