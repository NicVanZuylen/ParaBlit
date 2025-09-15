#include "Material.h"
#include "Texture.h"

namespace Eng
{
	Material::Material(AssetEncoder::AssetID* textureIDs, uint32_t textureCount) 
		: Ctrl::DataClass(this)
	{
		m_textureIDs.SetCount(textureCount);
		memcpy(m_textureIDs.Data(), textureIDs, sizeof(PB::ITexture*) * textureCount);
	}

	void Material::ResolveTextureIDs()
	{
		if (m_textureIDs.Count() == 0)
		{
			uint32_t i = 0;
			for (const std::string& name : m_textureNames)
			{
				if (name.empty() == false && name != "0")
				{
					AssetEncoder::AssetID id = AssetEncoder::AssetHandle(name.c_str()).GetID(&Texture::s_textureDatabaseLoader);
					m_textureIDs.PushBack(id);
				}
				else if (m_textureSimpleIDs[i] > 0)
				{
					AssetEncoder::AssetID id = ~AssetEncoder::AssetID(0) - m_textureSimpleIDs[i];
					m_textureIDs.PushBack(id);
				}

				++i;
			}
		}
	}
};
