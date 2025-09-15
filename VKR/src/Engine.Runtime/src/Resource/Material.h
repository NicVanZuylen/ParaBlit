#pragma once
#include "Material_generated.h"
#include "CLib/Vector.h"
#include "Engine.ParaBlit/ITexture.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.AssetEncoder/AssetDatabaseReader.h"

namespace Eng
{
	class Material : public Ctrl::DataClass
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
			: Ctrl::DataClass(this)
		{
		}

		Material(AssetEncoder::AssetID* textureIDs, uint32_t textureCount);

		~Material() = default;

		void ResolveTextureIDs();
		AssetEncoder::AssetID* GetTextureIDs() { return m_textureIDs.Data(); }

		uint32_t GetTextureCount() const { return m_textureIDs.Count(); }

	private:

		REFLECTRON_FIELD()
		std::string m_name;
		REFLECTRON_FIELD()
		std::string m_textureNames[8];
		REFLECTRON_FIELD()
		int m_textureSimpleIDs[8]
		{
			ESimpleTextureType::SOLID_WHITE,
			ESimpleTextureType::FLAT_NORMAL_MAP,
			ESimpleTextureType::SOLID_BLACK,
			ESimpleTextureType::SOLID_WHITE,
			ESimpleTextureType::SOLID_BLACK,
			ESimpleTextureType::NONE,
			ESimpleTextureType::NONE,
			ESimpleTextureType::NONE
		};


		CLib::Vector<AssetEncoder::AssetID, 8> m_textureIDs;
	};
	CLIB_REFLECTABLE_CLASS(Material)
};