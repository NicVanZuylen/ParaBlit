#include "Shader.h"
#include "Utility/QuickIO.h"
#include "CLib/Allocator.h"

#include <cstring>
#include <assert.h>
#include <filesystem>

namespace PBClient
{
	AssetEncoder::AssetBinaryDatabaseReader Shader::s_shaderDatabaseLoader;

	Shader::Shader(PB::IRenderer* renderer, const char* path, CLib::Allocator* allocator, bool loadFromDatabase)
	{
		PB::ShaderModuleDesc moduleDesc{};
		moduleDesc.m_key = path;
		moduleDesc.m_keySize = strlen(path);
		
		if ((m_module = renderer->GetShaderModuleCache()->GetModule(moduleDesc)) == 0)
		{
			char* data = nullptr;
			if (loadFromDatabase == false)
			{
				if (allocator)
					QIO::LoadAlloc(path, &data, &moduleDesc.m_size, allocator);
				else
					QIO::Load(path, &data, &moduleDesc.m_size);
			}
			else
			{
				constexpr const char* ShaderDatabaseDir = "/Assets/build/shaders.adb";
				if (s_shaderDatabaseLoader.HasOpenFile() == false)
				{
					std::string dbDir = std::move(std::filesystem::current_path().string());
					dbDir += ShaderDatabaseDir;
					s_shaderDatabaseLoader.OpenFile(dbDir.c_str());
				}

				const AssetEncoder::AssetMeta& assetInfo = s_shaderDatabaseLoader.GetAssetInfo(path);
				moduleDesc.m_size = assetInfo.m_binarySize;
				if (allocator)
					data = reinterpret_cast<char*>(allocator->Alloc(uint32_t(assetInfo.m_binarySize)));
				else
					data = new char[assetInfo.m_binarySize];

				s_shaderDatabaseLoader.GetAssetBinary(path, data);

				printf("Shader: Successfully loaded asset [%s] (%u bytes) from database: %s\n", path, uint32_t(assetInfo.m_binarySize), ShaderDatabaseDir);
			}

			moduleDesc.m_byteCode = data;
			m_module = renderer->GetShaderModuleCache()->GetModule(moduleDesc);
			if (!m_module)
			{
				printf("Could create shader module from file:%s\n", path);
				assert(false);
			}

			if (allocator)
				allocator->Free((void*)data);
			else
				delete[] data;
		}
	}

	Shader::~Shader()
	{

	}

	PB::ShaderModule Shader::GetModule()
	{
		return m_module;
	}

	Shader::operator PB::ShaderModule()
	{
		return m_module;
	}
}