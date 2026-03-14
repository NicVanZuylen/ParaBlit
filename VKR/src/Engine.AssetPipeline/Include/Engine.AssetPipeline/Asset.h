#pragma once
#include "Asset_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Engine.Control/IDataClass.h"

namespace AssetPipeline
{
	class Asset : public Ctrl::DataClass
	{
		REFLECTRON_CLASS()

	public:

		REFLECTRON_GENERATED_Asset()

        template<typename T>
        Asset(T* assetInstance, bool requiresAssetBinary = true) 
		: Ctrl::DataClass(assetInstance)
		{
            m_reflector.AddFields(this);
		}

		~Asset() = default;

		const std::string& GetAssetGUID() const { return m_assetGuid; }
		const std::string& GetAssetName() const { return m_assetName; }

		void SetAssetGUID(const Ctrl::GUID& guid) { m_assetGuid = guid.AsCString(); }
		void SetAssetName(const char* name) { m_assetName = name; }
        void SetAssetName(const std::string& name) { m_assetName = name; }

		virtual const char* GetDataClassInstanceName() override
		{
			if (!m_assetName.empty())
			{
				return m_assetName.c_str();
			}
			else if (!m_assetGuid.empty())
			{
				return m_assetGuid.c_str();
			}
			
			return "Unknown Asset";
		}

	protected:

		REFLECTRON_FIELD()
		std::string m_assetGuid;
		REFLECTRON_FIELD()
		std::string m_assetName;
	};
}