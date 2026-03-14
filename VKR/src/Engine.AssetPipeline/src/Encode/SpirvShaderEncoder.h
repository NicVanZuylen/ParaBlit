#pragma once
#include "Engine.AssetEncoder/EncoderBase.h"
#include "Engine.AssetEncoder/AssetBinaryDatabase.h"
#include "Engine.AssetEncoder/ShaderPermutation.h"
#include "CLib/Allocator.h"

#include <unordered_map>

struct shaderc_include_result;
struct shaderc_compilation_result;

namespace AssetPipeline
{

	class SpirvShaderEncoder : public AssetEncoder::EncoderBase
	{
	public:

		SpirvShaderEncoder(const char* name, const char* dbName, const char* assetDirectory);

		~SpirvShaderEncoder();

	private:

		struct IncludeData
		{
			std::unordered_map<std::string, CLib::Vector<char>> m_headerMap;
		};

		struct IncludeUserData
		{
			CLib::Allocator* m_allocator = nullptr;
			const AssetStatus* m_asset = nullptr;
			IncludeData* m_includeData = nullptr;
		};

		struct ShaderAssetIncludeInfo
		{
			struct FixedData
			{
				size_t m_headerLastModifiedTime = 0;
			} m_fixedData;
			std::string m_headerName;
		};

		static inline shaderc_include_result* IncludeResolveCallback(void* user_data, const char* requested_source, int type,
			const char* requesting_source, size_t include_depth);

		static inline void IncludeResultReleaseCallback(void* user_data, shaderc_include_result* include_result);

		// Recursively checks if any included shader files have been modified. Return false if a included file is out-dated.
		inline bool CheckShaderIncludes(const std::string& glsl, std::filesystem::path filePath, uint64_t lastBuildTime);

		struct PermutationMacroInfo
		{
			size_t definitionStart = 0;
			size_t definitionEnd = 0;
			size_t definitionValue = 0;
		};
		using PermutationList = std::vector<std::pair<std::string, PermutationMacroInfo>>;

		void FindPermutations(const std::string& glsl, PermutationList& outPermutations);

		void CompileAllPermutations(const AssetStatus& asset, const std::string& glsl, const PermutationList& permutations, uint8_t* currentState, uint32_t permutationIndex);
		void CompilePermutation(const AssetStatus& asset, const std::string& glsl, const PermutationList& permutations, uint8_t* currentPermutationState);

		inline void BuildShader(const AssetStatus& asset);

		CLib::Allocator m_allocator{ 16384 };
		IncludeData m_includeData{};
		CLib::Vector<std::pair<shaderc_compilation_result*, AssetEncoder::PermutationKey>, 0, 8, true> m_compiledPermutations;
	};
}

