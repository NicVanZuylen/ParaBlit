#include "Material.h"

namespace Eng
{
	Material::Material(PB::Pipeline pipeline, PB::ITexture** textures, uint32_t textureCount, PB::ResourceView sampler)
	{
		m_sampler = sampler;
		m_pipeline = pipeline;
		m_textures.SetCount(textureCount);
		memcpy(m_textures.Data(), textures, sizeof(PB::ITexture*) * textureCount);
		m_textureViews.Reserve(textureCount);
	}

	PB::BindingLayout Material::GetBindings()
	{
		for (uint32_t i = m_textureViews.Count(); i < m_textures.Count(); ++i)
		{
			m_textureViews.PushBack(m_textures[i]->GetDefaultSRV());
		}

		PB::BindingLayout outLayout;
		outLayout.m_uniformBufferCount = 0;
		outLayout.m_uniformBuffers = nullptr;
		outLayout.m_resourceCount = m_textureViews.Count();
		outLayout.m_resourceViews = m_textureViews.Data();

		return outLayout;
	}
};
