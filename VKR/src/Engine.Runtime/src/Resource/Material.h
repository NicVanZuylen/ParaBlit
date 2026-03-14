#pragma once
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Material_generated.h"
#include "CLib/Vector.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.AssetEncoder/AssetBinaryDatabaseReader.h"
#include "Engine.AssetPipeline/TextureAsset.h"

namespace Eng
{
	class Material : public AssetPipeline::Asset
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_Material()

		enum ESimpleTextureType : uint8_t
		{
			NONE,
			SOLID_BLACK,
			SOLID_WHITE,
			MID_GRAY,
			FLAT_NORMAL_MAP,
			COUNT
		};

		static inline int SimpleTextures[5] = 
		{ 
			ESimpleTextureType::SOLID_WHITE, 
			ESimpleTextureType::FLAT_NORMAL_MAP, 
			ESimpleTextureType::SOLID_BLACK, 
			ESimpleTextureType::SOLID_WHITE, 
			ESimpleTextureType::SOLID_BLACK
		};

		Material() 
			: AssetPipeline::Asset(this, false /* requresAssetBinary */)
		{
			
		}

		Material(AssetEncoder::AssetID* textureIDs, uint32_t textureCount);

		~Material() = default;

		void ResolveTextureIDs();
		void ReloadTextures();
		AssetEncoder::AssetID* GetTextureIDs() { return m_textureIDs.Data(); }

		uint32_t GetTextureCount() const { return m_textureIDs.Count(); }

		virtual const char* GetDataClassInstanceName() override final
		{
			if (m_assetName.empty())
			{
				return m_name.c_str();
			}
			else
			{
				return m_assetName.c_str();
			}
		}

		virtual void OnFieldChanged(const ReflectronFieldData& field) override final;

	private:

		static constexpr uint32_t MaxTextures = 8;

		REFLECTRON_FIELD()
		std::string m_name;
		REFLECTRON_FIELD()
		Ctrl::TObjectPtr<AssetPipeline::TextureAsset> m_textures[8];
		REFLECTRON_FIELD()
		int m_textureSimpleIDs[8]
		{
			SimpleTextures[0],
			SimpleTextures[1],
			SimpleTextures[2],
			SimpleTextures[3],
			SimpleTextures[4],
			ESimpleTextureType::NONE,
			ESimpleTextureType::NONE,
			ESimpleTextureType::NONE
		};

		static_assert(sizeof(m_textures) / sizeof(Ctrl::TObjectPtr<AssetPipeline::TextureAsset>) == MaxTextures);
		static_assert(sizeof(m_textureSimpleIDs) / sizeof(int) == MaxTextures);

		CLib::Vector<AssetEncoder::AssetID, 8> m_textureIDs;
	};
	CLIB_REFLECTABLE_CLASS(Material)
};