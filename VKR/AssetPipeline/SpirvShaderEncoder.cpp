#include "SpirvShaderEncoder.h"

#include <shaderc/shaderc.h>
#include <chrono>
#include <cassert>

namespace AssetPipeline
{
	SpirvShaderEncoder::SpirvShaderEncoder(const char* name, const char* dbName, const char* assetDirectory)
		: EncoderBase(name, dbName, assetDirectory)
	{
		std::vector<AssetEncoder::FileInfo> fileInfos;
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".vert");
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".frag");
		RecursiveSearchDirectoryForExtension(assetDirectory, fileInfos, ".comp");
		
		std::vector<AssetStatus> assetStatus;
		GetAssetStatus("Shaders", fileInfos, assetStatus);

		for (auto& asset : assetStatus)
		{
			if (asset.m_buildRequired)
			{
				BuildShader(asset);
			}
		}
	}

	SpirvShaderEncoder::~SpirvShaderEncoder()
	{
	}

// Unscoped enum from shaderc causes C26812.
#pragma warning(disable : 26812)

	inline void SpirvShaderEncoder::BuildShader(const AssetStatus& asset)
	{
		std::ifstream glslFile(asset.m_fullPath.c_str(), std::ios::ate | std::ios::binary | std::ios::_Nocreate | std::ios::_Noreplace);
		auto fileExceptions = glslFile.exceptions();
		assert(glslFile.good() && !fileExceptions);

		// Determine shader stage from file extension, much like glslangvalidator.exe does.
		shaderc_shader_kind stage = shaderc_shader_kind::shaderc_vertex_shader;
		if (asset.m_extension == ".frag")
			stage = shaderc_shader_kind::shaderc_fragment_shader;
		else if(asset.m_extension == ".comp")
			stage = shaderc_shader_kind::shaderc_compute_shader;

		// Generate options and set optimization level.
		shaderc_compile_options_t options = shaderc_compile_options_initialize();
		shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);

		// Read GLSL...
		size_t fileSize = glslFile.tellg();
		CLib::Vector<char> buf;
		buf.SetCount(uint32_t(fileSize));
		buf.PushBack('\0');

		glslFile.seekg(0);
		glslFile.read(buf.Data(), fileSize);

		shaderc_compiler_t compiler = shaderc_compiler_initialize();

		shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, buf.Data(), fileSize, stage, asset.m_fullPath.c_str(), "main", options);
		size_t errorCount = shaderc_result_get_num_errors(result);
		if (errorCount)
		{
			printf("%s: shaderc error: %s\n", m_name.c_str(), shaderc_result_get_error_message(result));
		}
		else
		{
			printf("%s: Successfully compiled shader: %s\n", m_name.c_str(), asset.m_fullPath.c_str());
		}

		assert(errorCount == 0);

		size_t binarySize = shaderc_result_get_length(result);
		void* dstStorage = m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), binarySize, asset.m_info.m_dateModified);
		memcpy(dstStorage, shaderc_result_get_bytes(result), binarySize);

		printf("%s: Stored compiled shader %s in database: %s at location: %s\n", m_name.c_str(), asset.m_fullPath.c_str(), m_dbName.c_str(), asset.m_dbPath.c_str());

		shaderc_result_release(result);

		// Release resources.
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);
	}
}