#include "Material.h"

namespace Eng
{
	Material::Material(PB::Pipeline pipeline, AssetEncoder::AssetID* textureIDs, uint32_t textureCount, PB::ResourceView sampler)
	{
		m_sampler = sampler;
		m_pipeline = pipeline;
		m_textureIDs.SetCount(textureCount);
		memcpy(m_textureIDs.Data(), textureIDs, sizeof(PB::ITexture*) * textureCount);
	}
};
