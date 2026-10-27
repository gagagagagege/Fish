#include "VulkanTexture2D.h"

#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "VulkanRendererAPI.h"

#include <utility>

namespace Fish {
	VulkanTexture2D::VulkanTexture2D(VulkanContext* context, CommandPool& transientPool,
		VulkanRendererAPI* owner, const std::string& path)
		: m_owner(owner)
	{
		m_texture = texture::loadFromFile(context, transientPool, path.c_str());
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
		// 只还槽位号不够 —— image/view/sampler 必须在命令缓冲引用它们期间活着。
		// 整个 texture 交给后端,由它等两帧再销毁。
		if (m_owner != nullptr)
			m_owner->ReleaseTexture(std::move(m_texture), m_imageIndex, m_samplerIndex);
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
