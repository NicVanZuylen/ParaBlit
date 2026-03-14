#pragma once
#include "Asset.h"
#include "TextureAsset_generated.h"
#include "CLib/Reflection.h"

namespace AssetPipeline
{
	class TextureAsset : public Asset
	{
		REFLECTRON_CLASS()

	public:

		REFLECTRON_GENERATED_TextureAsset()

		TextureAsset() : Asset(this)
		{
		}

		~TextureAsset() = default;

		const std::string& GetIrradianceAssetGUID() const { return m_irradianceAssetGuid; }
		const std::string& GetPrefilterAssetGUID() const { return m_prefilterAssetGuid; }
		bool GetIgnoreHDR() const { return m_ignoreHDR; }
		bool GetIsSkybox() const { return m_isSkybox; }
		bool GetIsEnvironmentMap() const { return m_isEnvironmentMap; }
		bool GetIsCompressed() const { return m_isCompressed; }
		bool GetSRGB() const { return m_sRGB; }
		int GetMipCount() const { return m_mipCount; }
		int GetArrayCount() const { return m_arrayCount; }
		const std::string& GetCompressionFormat() const { return m_compressionFormat; }

		void SetIrradianceAssetGUID(const Ctrl::GUID& guid) { m_irradianceAssetGuid = guid.AsCString(); }
		void SetPrefilterAssetGUID(const Ctrl::GUID& guid) { m_prefilterAssetGuid = guid.AsCString(); }
		void SetIgnoreHDR(bool value) { m_ignoreHDR = value; }
		void SetIsSkybox(bool value) { m_isSkybox = value; }
		void SetIsEnvironmentMap(bool value) { m_isEnvironmentMap = value; }
		void SetIsCompressed(bool value) { m_isCompressed = value; }
		void SetSRGB(bool value) { m_sRGB = value; }
		void SetMipCount(int value) { m_mipCount = value; }
		void SetArrayCount(int value) { m_arrayCount = value; }
		void SetCompressionFormat(const std::string& value) { m_compressionFormat = value; }

	private:

		REFLECTRON_FIELD()
		std::string m_irradianceAssetGuid;
		REFLECTRON_FIELD()
		std::string m_prefilterAssetGuid;
		REFLECTRON_FIELD()
		bool m_ignoreHDR = false;
		REFLECTRON_FIELD()
		bool m_isSkybox = false;
		REFLECTRON_FIELD()
		bool m_isEnvironmentMap = false;
		REFLECTRON_FIELD()
		bool m_isCompressed = true;
		REFLECTRON_FIELD()
		bool m_sRGB = false;
		REFLECTRON_FIELD(min=1, max=8)
		int m_mipCount = 1;
		REFLECTRON_FIELD(min=1, max=8)
		int m_arrayCount = 1;
		REFLECTRON_FIELD()
		std::string m_compressionFormat = "BC7";
	};
	CLIB_REFLECTABLE_CLASS(TextureAsset)
}