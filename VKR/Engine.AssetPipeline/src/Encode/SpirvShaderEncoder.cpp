#include "SpirvShaderEncoder.h"

#include <shaderc/shaderc.h>
#include <chrono>
#include <cassert>

namespace AssetPipeline
{
	SpirvShaderEncoder::SpirvShaderEncoder(const char* name, const char* dbName, const char* assetDirectory)
		: EncoderBase(name, dbName, assetDirectory)
	{
		// Header files
		std::vector<AssetEncoder::FileInfo> headerFileInfos;
		RecursiveSearchDirectoryForExtension(assetDirectory, headerFileInfos, ".h");

		// Shader files
		std::vector<AssetEncoder::FileInfo> shaderFileInfos;
		RecursiveSearchDirectoryForExtension(assetDirectory, shaderFileInfos, ".vert");
		RecursiveSearchDirectoryForExtension(assetDirectory, shaderFileInfos, ".frag");
		RecursiveSearchDirectoryForExtension(assetDirectory, shaderFileInfos, ".comp");
		RecursiveSearchDirectoryForExtension(assetDirectory, shaderFileInfos, ".task");
		RecursiveSearchDirectoryForExtension(assetDirectory, shaderFileInfos, ".mesh");
		
		std::vector<AssetStatus> shaderAssetStatus;
		GetAssetStatus("Shaders", shaderFileInfos, shaderAssetStatus);

		for (auto& asset : shaderAssetStatus)
		{
			BuildShader(asset);
		}
	}

	SpirvShaderEncoder::~SpirvShaderEncoder()
	{
	}

// Unscoped enum from shaderc causes C26812.
#pragma warning(disable : 26812)

	shaderc_include_result* SpirvShaderEncoder::IncludeResolveCallback(void* user_data, const char* requested_source, int type,
		const char* requesting_source, size_t include_depth)
	{
		auto* data = reinterpret_cast<SpirvShaderEncoder::IncludeUserData*>(user_data);
		auto* asset = data->m_asset;

		std::filesystem::path includePath = requesting_source;
		includePath.remove_filename();
		includePath += requested_source;
		std::string pathString = includePath.string();
		char* pathCString = reinterpret_cast<char*>(data->m_allocator->Alloc(static_cast<uint32_t>(pathString.size()) + 1));
		memcpy(pathCString, pathString.data(), pathString.size() + 1);

		CLib::Vector<char>* content = nullptr;
		auto& headerMap = data->m_includeData->m_headerMap;
		auto it = headerMap.find(pathString);
		if (it == headerMap.end())
		{
			content = &(headerMap[pathString] = {});

			// Load include file.
			std::ifstream includeFile(pathString, std::ios::ate | std::ios::binary);
			if (!includeFile.good())
			{
				printf_s("[%s]: Include file not found: %s\n", requesting_source, pathString.c_str());
				assert(false);
			}

			size_t fileLength = includeFile.tellg();
			content->SetCount(uint32_t(fileLength));
			includeFile.seekg(0);
			includeFile.read(content->Data(), fileLength);
		}
		else
			content = &it->second;

		assert(content);

		shaderc_include_result* res = data->m_allocator->Alloc<shaderc_include_result>();

		res->source_name = pathCString;
		res->source_name_length = pathString.size();
		res->content = content->Data();
		res->content_length = content->Count();
		res->user_data = user_data;

		return res;
	}

	void SpirvShaderEncoder::IncludeResultReleaseCallback(void* user_data, shaderc_include_result* include_result)
	{
		auto* data = reinterpret_cast<SpirvShaderEncoder::IncludeUserData*>(user_data);
		data->m_allocator->Free(const_cast<char*>(include_result->source_name));
		//data->m_allocator->Free(include_result);
	}

	bool SpirvShaderEncoder::CheckShaderIncludes(const CLib::Vector<char>& glsl, std::filesystem::path filePath, uint64_t lastBuildTime)
	{
		filePath.remove_filename();
		std::string glslStr = glsl.Data();

		size_t pos = 0;
		while (pos != std::string::npos)
		{
			pos = glslStr.find("#include", pos);
			if (pos != std::string::npos)
			{
				size_t leftQuote = glslStr.find_first_of("\"", pos);
				size_t rightQuote = glslStr.find_first_of("\"", leftQuote + 1);

				std::filesystem::path includePath = filePath;
				includePath.append(glslStr.substr(leftQuote + 1, rightQuote - leftQuote - 1));
				
				std::filesystem::directory_entry entry(includePath);
				uint64_t lastModifiedTime = std::chrono::duration_cast<std::chrono::seconds>(entry.last_write_time().time_since_epoch()).count();

				// Early exit if this file is out-dated.
				if (lastModifiedTime > lastBuildTime)
				{
					return false;
				}

				// Check include files included by this header.
				std::ifstream glslFile(includePath, std::ios::ate | std::ios::binary | std::ios::_Nocreate | std::ios::_Noreplace | std::ios::in);
				auto fileExceptions = glslFile.exceptions();
				assert(glslFile.good() && !fileExceptions);

				size_t fileSize = glslFile.tellg();
				CLib::Vector<char> buf;
				buf.SetCount(uint32_t(fileSize));
				buf.PushBack('\0');

				glslFile.seekg(0);
				glslFile.read(buf.Data(), fileSize);
				glslFile.close();

				if (!CheckShaderIncludes(buf, includePath, lastBuildTime))
				{
					return false;
				}

				pos = rightQuote;
			}
		}

		return true;
	}

	void SpirvShaderEncoder::BuildShader(const AssetStatus& asset)
	{
		static constexpr bool GetAssembly = false;

		std::ifstream glslFile(asset.m_fullPath.c_str(), std::ios::ate | std::ios::binary | std::ios::_Nocreate | std::ios::_Noreplace | std::ios::in);
		auto fileExceptions = glslFile.exceptions();
		assert(glslFile.good() && !fileExceptions);

		// Read GLSL...
		size_t fileSize = glslFile.tellg();
		CLib::Vector<char> buf;
		buf.SetCount(uint32_t(fileSize));
		buf.PushBack('\0');

		glslFile.seekg(0);
		glslFile.read(buf.Data(), fileSize);
		glslFile.close();

		if (!asset.m_outdated && CheckShaderIncludes(buf, asset.m_fullPath, asset.m_info.m_dateBuilt))
		{
			WriteUnmodifiedAsset(asset);
			return;
		}
		else
		{
			if (!asset.m_outdated)
			{
				printf("%s: Included header changes detected for asset: %s Recompiling...\n", m_name.c_str(), asset.m_fullPath.c_str());
			}

			FlagAsModified();
		}

		// Determine shader stage from file extension, much like glslangvalidator does.
		shaderc_shader_kind stage = shaderc_shader_kind::shaderc_vertex_shader;
		if (asset.m_extension == ".frag")
			stage = shaderc_shader_kind::shaderc_fragment_shader;
		else if (asset.m_extension == ".comp")
			stage = shaderc_shader_kind::shaderc_compute_shader;
		if (asset.m_extension == ".task")
			stage = shaderc_shader_kind::shaderc_task_shader;
		else if (asset.m_extension == ".mesh")
			stage = shaderc_shader_kind::shaderc_mesh_shader;

		IncludeUserData includeUserData{};
		includeUserData.m_allocator = &m_allocator;
		includeUserData.m_asset = &asset;
		includeUserData.m_includeData = &m_includeData;

		// Generate options and set optimization level.
		shaderc_compile_options_t options = shaderc_compile_options_initialize();
		shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_4);
		shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
		shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);
		shaderc_compile_options_set_include_callbacks(options, IncludeResolveCallback, IncludeResultReleaseCallback, &includeUserData);

		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, buf.Data(), fileSize, stage, asset.m_fullPath.c_str(), "main", options);
		size_t errorCount = shaderc_result_get_num_errors(result);
		size_t warningCount = shaderc_result_get_num_warnings(result);
		shaderc_compilation_status status = shaderc_result_get_compilation_status(result);
		if (errorCount > 0)
		{
			printf("%s: shaderc error: %s\n", m_name.c_str(), shaderc_result_get_error_message(result));
		}
		else if (status != shaderc_compilation_status_success)
		{
			printf("%s: shaderc error: %s\n", m_name.c_str(), "Unknown error.");
		}
		else
		{
			if(warningCount > 0)
				printf("%s: Successfully compiled shader: %s with %u warnings.\n", m_name.c_str(), asset.m_fullPath.c_str(), uint32_t(warningCount));
			else
				printf("%s: Successfully compiled shader: %s\n", m_name.c_str(), asset.m_fullPath.c_str());
		}

		assert(errorCount == 0 && status == shaderc_compilation_status_success);
		size_t binarySize = shaderc_result_get_length(result);
		const char* shaderBytes = shaderc_result_get_bytes(result);
		void* dstStorage = m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), 0, binarySize, asset.m_lastModifiedTime);
		memcpy(dstStorage, shaderBytes, binarySize);

		if constexpr(GetAssembly)
		{
			shaderc_compilation_result_t disassemblyResult = shaderc_compile_into_spv_assembly(compiler, buf.Data(), fileSize, stage, asset.m_fullPath.c_str(), "main", options);
			const char* assembly = shaderc_result_get_bytes(disassemblyResult);
			(void)assembly;

			shaderc_result_release(disassemblyResult);
		}

		printf("%s: Stored compiled shader %s in database: %s at location: %s\n", m_name.c_str(), asset.m_fullPath.c_str(), m_dbName.c_str(), asset.m_dbPath.c_str());

		shaderc_result_release(result);

		// Release resources.
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);
	}
}