#include "Material.h"
#include "Engine.AssetPipeline/TextureAsset.h"
#include "Engine.Control/IDataClass.h"
#include "Texture.h"

#include <cstring>

namespace Eng
{
	Material::Material(AssetEncoder::AssetID* textureIDs, uint32_t textureCount) 
		: AssetPipeline::Asset(this, false /* requresAssetBinary */)
	{
		m_textureIDs.SetCount(textureCount);
		memcpy(m_textureIDs.Data(), textureIDs, sizeof(PB::ITexture*) * textureCount);
	}

	void Material::ResolveTextureIDs()
	{
		if (m_textureIDs.Count() == 0)
		{
			for (uint32_t i = 0; i < MaxTextures; ++i)
			{
				const Ctrl::TObjectPtr<AssetPipeline::TextureAsset>& asset = m_textures[i];

				if(asset != nullptr)
				{
					const char* assetGUID = asset->GetAssetGUID().c_str();

					AssetEncoder::AssetID id = AssetEncoder::AssetHandle(assetGUID).GetID(&Texture::s_textureDatabaseLoader);
					m_textureIDs.PushBack(id);
				}
				else if (m_textureSimpleIDs[i] > 0)
				{
					AssetEncoder::AssetID id = ~AssetEncoder::AssetID(0) - m_textureSimpleIDs[i];
					m_textureIDs.PushBack(id);
				}
			}
		}
	}

	void Material::ReloadTextures()
	{
		m_textureIDs.Clear();
		ResolveTextureIDs();
	}

	void Material::OnFieldChanged(const ReflectronFieldData& field)
	{
		if (strcmp(field.m_name, "m_textures") == 0)
		{
			ReloadTextures();
		}
	}
};
