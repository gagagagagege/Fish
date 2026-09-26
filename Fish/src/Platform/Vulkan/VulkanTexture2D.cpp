#include "VulkanTexture2D.h"

#include "VulkanContext.h"
#include "VulkanCommandPool.h"

namespace Fish {
	VulkanTexture2D::VulkanTexture2D(VulkanContext* context, CommandPool& transientPool, const std::string& path)
	{
		m_texture = texture::loadFromFile(context, transientPool, path.c_str());
	}

	uint32_t VulkanTexture2D::GetWidth() const
	{
		return m_texture.getImage().getExtent().width;
	}

	uint32_t VulkanTexture2D::GetHeight() const
	{
		return m_texture.getImage().getExtent().height;
	}
}
