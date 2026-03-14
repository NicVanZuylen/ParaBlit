#include "Shader.h"
#include "Utility/QuickIO.h"
#include "CLib/Allocator.h"
#include "CLib/String.h"

#include <cstring>
#include <assert.h>
#include <filesystem>

namespace Eng
{
	AssetEncoder::AssetBinaryDatabaseReader Shader::s_shaderDatabaseLoader;

	Shader::Shader(PB::IRenderer* renderer, const char* path, AssetEncoder::PermutationKey permutationKey, CLib::Allocator* allocator, bool loadFromDatabase)
	{
		constexpr const char* ShaderDatabaseDir = "/Assets/build/shaders.adb";
		std::hash<const char*> pathHash;

		PB::ShaderModuleDesc moduleDesc{};
		moduleDesc.m_key[0] = pathHash(path);
		moduleDesc.m_key[1] = permutationKey;
		
		if ((m_module = renderer->GetShaderModuleCache()->GetModule(moduleDesc)) == 0)
		{
			char* data = nullptr;
			AssetEncoder::ShaderHeader header;

			if (loadFromDatabase == false)
			{
				if (allocator)
					QIO::LoadAlloc(path, &data, &moduleDesc.m_size, allocator);
				else
					QIO::Load(path, &data, &moduleDesc.m_size);
			}
			else
			{
				if (s_shaderDatabaseLoader.HasOpenFile() == false)
				{
					std::string dbDir = std::move(std::filesystem::current_path().string());
					dbDir += ShaderDatabaseDir;
					s_shaderDatabaseLoader.OpenFile(dbDir.c_str());
				}

				AssetEncoder::AssetHandle handle(path);
				const AssetEncoder::AssetMeta& assetInfo = s_shaderDatabaseLoader.GetAssetInfo(handle);
				if (allocator)
					data = reinterpret_cast<char*>(allocator->Alloc(uint32_t(assetInfo.m_binarySize)));
				else
					data = new char[assetInfo.m_binarySize];

				s_shaderDatabaseLoader.GetAssetBinary(handle, data);

				header.Deserialize(reinterpret_cast<const uint8_t*>(data));
			}

			size_t chosenPermutationBinarySize = 0;
			AssetEncoder::PermutationData chosenPermutation{};
			for (uint32_t i = 0; i < header.m_data.m_permutationCount; ++i)
			{
				const AssetEncoder::PermutationData& permData = *header.GetPermutation(i);

				if (permData.m_key == permutationKey)
				{
					chosenPermutation = permData;
					if (i < header.m_data.m_permutationCount - 1)
					{
						chosenPermutationBinarySize = header.GetPermutation(i + 1)->m_binaryOffsetBytes - chosenPermutation.m_binaryOffsetBytes;
					}
					else
					{
						chosenPermutationBinarySize = header.m_data.m_shaderBinarySize - chosenPermutation.m_binaryOffsetBytes;
					}

					break;
				}
			}

			moduleDesc.m_byteCode = data + header.GetByteCodeOffset() + chosenPermutation.m_binaryOffsetBytes;
			moduleDesc.m_size = chosenPermutationBinarySize;
			m_module = renderer->GetShaderModuleCache()->GetModule(moduleDesc);
			if (!m_module)
			{
				printf("Could create shader module from file:%s\n", path);
				assert(false);
			}
			else
			{
				printf("Shader: Successfully loaded shader asset [%s], PERMUTATION: %llX, (%llu bytes) from database: %s\n", path, permutationKey, chosenPermutationBinarySize, ShaderDatabaseDir);
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