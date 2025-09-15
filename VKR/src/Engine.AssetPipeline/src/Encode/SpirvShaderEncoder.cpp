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
		RecursiveSearchDirectoryForExtension(assetDirectory, shaderFileInfos, ".glsl");
		RecursiveSearchDirectoryForExtension(assetDirectory, shaderFileInfos, ".rgen");
		
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

	bool SpirvShaderEncoder::CheckShaderIncludes(const std::string& glsl, std::filesystem::path filePath, uint64_t lastBuildTime)
	{
		filePath.remove_filename();

		size_t pos = 0;
		while (pos != std::string::npos)
		{
			pos = glsl.find("#include", pos);
			if (pos != std::string::npos)
			{
				size_t leftQuote = glsl.find_first_of("\"", pos);
				size_t rightQuote = glsl.find_first_of("\"", leftQuote + 1);

				std::filesystem::path includePath = filePath;
				includePath.append(glsl.substr(leftQuote + 1, rightQuote - leftQuote - 1));
				
				std::filesystem::directory_entry entry(includePath);
				uint64_t lastModifiedTime = std::chrono::duration_cast<std::chrono::seconds>(entry.last_write_time().time_since_epoch()).count();

				// Early exit if this file is out-dated.
				if (lastModifiedTime > lastBuildTime)
				{
					return false;
				}

				// Check include files included by this header.
				std::ifstream glslFile(includePath, std::ios::ate | std::ios::binary | std::ios::_Nocreate | std::ios::in);
				auto fileExceptions = glslFile.exceptions();
				assert(glslFile.good() && !fileExceptions);

				size_t fileSize = glslFile.tellg();
				CLib::Vector<char> buf;
				buf.SetCount(uint32_t(fileSize));
				buf.PushBack('\0');

				glslFile.seekg(0);
				glslFile.read(buf.Data(), fileSize);
				glslFile.close();

				std::string bufStr = buf.Data();

				if (!CheckShaderIncludes(bufStr, includePath, lastBuildTime))
				{
					return false;
				}

				pos = rightQuote;
			}
		}

		return true;
	}

	void RemoveWhitespace(std::string& str)
	{
		str.erase(std::remove(str.begin(), str.end(), '\t'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\v'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\f'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
	}

	void SpirvShaderEncoder::FindPermutations(const std::string& glsl, PermutationList& outPermutations)
	{
		const std::string whiteSpace = " \t\n\v\f\r";

		size_t definePos = 0;
		while (definePos != std::string::npos)
		{
			definePos = glsl.find("#define", definePos);

			if (definePos != std::string::npos)
			{
				size_t defineEnd = glsl.find_first_of(whiteSpace, definePos) + 1;
				assert(defineEnd != std::string::npos);
				size_t macroNameStart = glsl.find_first_not_of(whiteSpace, defineEnd);

				size_t macroNameEnd = glsl.find_first_of(whiteSpace, macroNameStart);
				assert(macroNameEnd != std::string::npos);

				std::string macroName = glsl.substr(macroNameStart, macroNameEnd - macroNameStart);
				if (macroName.find("PERMUTATION_") == 0) // Name must start with "PERMUTATION_"
				{
					// A permutation declaration has been found.

					size_t valueStart = glsl.find_first_of(whiteSpace, macroNameEnd);
					size_t valueEnd = glsl.find_first_of('\n', valueStart + 1);

					std::string valueStr = glsl.substr(valueStart, valueEnd - valueStart);
					valueStr.erase(std::remove(valueStr.begin(), valueStr.end(), '\\'), valueStr.end()); // Also remove backslashes.
					RemoveWhitespace(valueStr);

					size_t permutationCount = std::atoi(valueStr.c_str());
					assert(permutationCount > 0 && "Shader declares permutation but it does not contain any possible values.");

					PermutationMacroInfo macroInfo{};
					macroInfo.definitionStart = definePos;
					macroInfo.definitionEnd = valueEnd;
					macroInfo.definitionValue = permutationCount;

					outPermutations.push_back({ macroName, macroInfo });

					definePos = valueEnd;
				}

				++definePos;
			}
		}
	}

	void SpirvShaderEncoder::CompileAllPermutations(const AssetStatus& asset, const std::string& glsl, const PermutationList& permutations, uint8_t* currentState, uint32_t permutationIndex)
	{
		auto& permutationInfo = permutations[permutationIndex];
		for (uint32_t i = 0; i < permutationInfo.second.definitionValue; ++i)
		{
			currentState[permutationIndex] = i;
			if (permutationIndex < permutations.size() - 1)
			{
				CompileAllPermutations(asset, glsl, permutations, currentState, permutationIndex + 1);
			}
			else
			{
				CompilePermutation(asset, glsl, permutations, currentState);
			}
		}
	}

	void SpirvShaderEncoder::CompilePermutation(const AssetStatus& asset, const std::string& glsl, const PermutationList& permutations, uint8_t* currentPermutationState)
	{
		static constexpr bool GetAssembly = false;

		// Determine shader stage from file extension, much like glslangvalidator does.
		shaderc_shader_kind stage = shaderc_shader_kind::shaderc_vertex_shader;
		bool determineStageWithPermutation = false;
		bool determineRTStageWithPermutation = false;
		if (asset.m_extension == ".vert")
			stage = shaderc_shader_kind::shaderc_vertex_shader;
		else if (asset.m_extension == ".frag")
			stage = shaderc_shader_kind::shaderc_fragment_shader;
		else if (asset.m_extension == ".comp")
			stage = shaderc_shader_kind::shaderc_compute_shader;
		else if (asset.m_extension == ".task")
			stage = shaderc_shader_kind::shaderc_task_shader;
		else if (asset.m_extension == ".mesh")
			stage = shaderc_shader_kind::shaderc_mesh_shader;
		else if (asset.m_extension == ".glsl")
			determineStageWithPermutation = true;
		else if (asset.m_extension == ".rgen")
			determineRTStageWithPermutation = true;

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

		AssetEncoder::ShaderPermutationTable permTable{};

		// Set current permutation macro values and generate key.
		printf_s("%s: Compiling permutation with values:\n", m_name.c_str());
		bool foundStagePermutation = determineStageWithPermutation == false && determineRTStageWithPermutation == false;
		for (uint32_t j = 0; j < permutations.size(); ++j)
		{
			auto& p = permutations[j];
			std::string valueStr = std::to_string(currentPermutationState[j]);

			shaderc_compile_options_add_macro_definition(options, p.first.c_str(), p.first.size(), valueStr.c_str(), valueStr.size());

			permTable.SetPermutation(AssetEncoder::EDefaultPermutationID(j), currentPermutationState[j]);

			static constexpr const char* ShaderStagePermutationName = "PERMUTATION_ShaderStage";
			static constexpr const char* RTShaderStagePermutationName = "PERMUTATION_RT_ShaderStage";
			if (determineStageWithPermutation == true && p.first == ShaderStagePermutationName)
			{
				printf_s("> NOTE: %s is a reserved permutation name for selecting which shader stage (non-raytracing) to compile.\n", ShaderStagePermutationName);

				const char* stageStr = "UNKNOWN_STAGE";
				switch (AssetEncoder::EShaderStagePermutation(currentPermutationState[j]))
				{
				case AssetEncoder::EShaderStagePermutation::VERTEX:
					stage = shaderc_shader_kind::shaderc_vertex_shader;
					stageStr = "VERTEX";
					break;
				case AssetEncoder::EShaderStagePermutation::FRAGMENT:
					stage = shaderc_shader_kind::shaderc_fragment_shader;
					stageStr = "FRAGMENT";
					break;
				case AssetEncoder::EShaderStagePermutation::COMPUTE:
					stage = shaderc_shader_kind::shaderc_compute_shader;
					stageStr = "COMPUTE";
					break;
				case AssetEncoder::EShaderStagePermutation::TASK:
					stage = shaderc_shader_kind::shaderc_task_shader;
					stageStr = "TASK";
					break;
				case AssetEncoder::EShaderStagePermutation::MESH:
					stage = shaderc_shader_kind::shaderc_mesh_shader;
					stageStr = "MESH";
					break;
				}

				printf_s("> %s: %s (%u)\n", p.first.c_str(), stageStr, uint32_t(currentPermutationState[j]));
				foundStagePermutation = true;
			}
			else if (determineRTStageWithPermutation == true && p.first == RTShaderStagePermutationName)
			{
				printf_s("> NOTE: %s is a reserved permutation name for selecting which shader stage (ray tracing) to compile.\n", RTShaderStagePermutationName);

				const char* stageStr = "UNKNOWN_STAGE";
				switch (AssetEncoder::ERTShaderStagePermutation(currentPermutationState[j]))
				{
				case AssetEncoder::ERTShaderStagePermutation::RAYGEN:
					stage = shaderc_shader_kind::shaderc_raygen_shader;
					stageStr = "RAYGEN";
					break;
				case AssetEncoder::ERTShaderStagePermutation::MISS:
					stage = shaderc_shader_kind::shaderc_miss_shader;
					stageStr = "MISS";
					break;
				case AssetEncoder::ERTShaderStagePermutation::CLOSESTHIT:
					stage = shaderc_shader_kind::shaderc_closesthit_shader;
					stageStr = "CLOSESTHIT";
					break;
				case AssetEncoder::ERTShaderStagePermutation::ANYHIT:
					stage = shaderc_shader_kind::shaderc_anyhit_shader;
					stageStr = "ANYHIT";
					break;
				case AssetEncoder::ERTShaderStagePermutation::INTERSECTION:
					stage = shaderc_shader_kind::shaderc_intersection_shader;
					stageStr = "INTERSECTION";
					break;
				}

				printf_s("> %s: %s (%u)\n", p.first.c_str(), stageStr, uint32_t(currentPermutationState[j]));
				foundStagePermutation = true;
			}
			else
			{
				printf_s("> %s: %u\n", p.first.c_str(), uint32_t(currentPermutationState[j]));
			}
		}

		assert(foundStagePermutation == true && "Shader stage permutation (PERMUTATION_ShaderStage) expected but not found!");

		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, glsl.c_str(), glsl.size(), stage, asset.m_fullPath.c_str(), "main", options);
		size_t errorCount = shaderc_result_get_num_errors(result);
		size_t warningCount = shaderc_result_get_num_warnings(result);
		shaderc_compilation_status status = shaderc_result_get_compilation_status(result);

		m_compiledPermutations.PushBack({ result, permTable.GetKey() });

		if (errorCount == 0 && status == shaderc_compilation_status_success)
		{
			if (warningCount > 0)
				printf_s("%s: Successfully compiled shader permutation: %s with %u warnings.\n", m_name.c_str(), asset.m_fullPath.c_str(), uint32_t(warningCount));
			else
				printf_s("%s: Successfully compiled shader permutation: %s\n", m_name.c_str(), asset.m_fullPath.c_str());

			if constexpr (GetAssembly) // Debug
			{
				shaderc_compilation_result_t disassemblyResult = shaderc_compile_into_spv_assembly(compiler, glsl.c_str(), glsl.size(), stage, asset.m_fullPath.c_str(), "main", options);
				const char* assembly = shaderc_result_get_bytes(disassemblyResult);
				(void)assembly;

				shaderc_result_release(disassemblyResult);
			}
		}
		else
		{
			bool expectedFailure = false;
			if (errorCount > 0)
			{
				const char* errorMessage = shaderc_result_get_error_message(result);
				if (errorCount == 1 && (determineStageWithPermutation || determineRTStageWithPermutation) == true && std::strstr(errorMessage, "Missing entry point: Each stage requires one entry point"))
				{
					// Expected failure, the shader does not include the stage being compiled from the current permutation (no entry point for stage).
					printf_s("%s: No entry point found, emitting null shader permutation.\n", m_name.c_str());
					expectedFailure = true;
				}
				else
				{
					printf_s("%s: shaderc error: %s\n", m_name.c_str(), errorMessage);
				}
			}
			else if (status != shaderc_compilation_status_success)
			{
				printf_s("%s: shaderc error: %s\n", m_name.c_str(), "Unknown error.");
			}

			if (expectedFailure == false)
			{
				printf_s("%s: Failed to compile shader permutation: %s\n", m_name.c_str(), asset.m_fullPath.c_str());
				assert(false);
			}
		}

		// Release resources.
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);
	}

	void SpirvShaderEncoder::BuildShader(const AssetStatus& asset)
	{
		static constexpr bool GetAssembly = false;

		std::string glsl;
		{
			std::ifstream glslFile(asset.m_fullPath.c_str(), std::ios::ate | std::ios::_Nocreate | std::ios::binary | std::ios::in);
			auto fileExceptions = glslFile.exceptions();
			assert(glslFile.good() && !fileExceptions);

			// Read GLSL...
			size_t fileSize = glslFile.tellg();
			glsl.resize(fileSize);

			glslFile.seekg(0);
			glslFile.read(glsl.data(), fileSize);
			glslFile.close();
		}

		if (!asset.m_outdated && CheckShaderIncludes(glsl, asset.m_fullPath, asset.m_info.m_dateBuilt))
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

		// Find shader permutations.
		PermutationList permutations;
		FindPermutations(glsl, permutations);

		// Erase macro declarations from glsl to be re-declared with appropriate values when compiling each shader permutation.
		for (auto it = permutations.rbegin(); it != permutations.rend(); ++it)
		{
			PermutationMacroInfo macroInfo = it->second;
			glsl.erase(macroInfo.definitionStart, macroInfo.definitionEnd - macroInfo.definitionStart);
		}

		if(permutations.size() == 0)
		{
			// Add a single permutation to compile the shader once.
			PermutationMacroInfo permutationInfo;
			permutationInfo.definitionEnd = permutationInfo.definitionStart = 0;
			permutationInfo.definitionValue = 1;
			permutations.push_back({ "PERMUTATION_Default", permutationInfo });
		}

		// Compile once for each permutation.
		CLib::Vector<uint8_t> permutationState;
		permutationState.SetCount(permutations.size());

		CompileAllPermutations(asset, glsl, permutations, permutationState.Data(), 0);

		size_t currentPermutationOffset = 0;
		std::vector<AssetEncoder::PermutationData> permutationData;
		std::vector<uint8_t> permutationBinaryBlob;
		for (auto& p : m_compiledPermutations)
		{
			shaderc_compilation_result* result = p.first;
			if (shaderc_result_get_compilation_status(result) == shaderc_compilation_status_success)
			{
				size_t permutationSize = shaderc_result_get_length(result);

				AssetEncoder::PermutationData permData;
				permData.m_key = p.second;
				permData.m_binaryOffsetBytes = currentPermutationOffset;
				permData.m_binarySize = permutationSize;
				permutationData.push_back(permData);

				permutationBinaryBlob.resize(permutationBinaryBlob.size() + permutationSize);
				std::memcpy(permutationBinaryBlob.data() + currentPermutationOffset, shaderc_result_get_bytes(result), permutationSize);

				currentPermutationOffset += permutationSize;
			}
			else // Null permutation for expected failures.
			{
				AssetEncoder::PermutationData permData;
				permData.m_key = p.second;
				permData.m_binaryOffsetBytes = currentPermutationOffset; // Provide offset to aid in calculating size for other non-null permutations.
				permData.m_binarySize = 0;
				permutationData.push_back(permData);
			}
		}

		AssetEncoder::ShaderHeader header{};

		size_t dataSize = AssetEncoder::ShaderHeader::FixedHeaderSize + (permutationData.size() * sizeof(AssetEncoder::PermutationData)) + permutationBinaryBlob.size();
		void* dstStorage = m_dbWriter->AllocateAsset(asset.m_dbPath.c_str(), 0, dataSize, asset.m_lastModifiedTime);
		header.Serialize
		(
			reinterpret_cast<uint8_t*>(dstStorage),
			permutationData.data(),
			permutationData.size(),
			permutationBinaryBlob.data(),
			permutationBinaryBlob.size()
		);

		for (auto& p : m_compiledPermutations)
		{
			shaderc_result_release(p.first);
		}
		m_compiledPermutations.Clear();
	}
}