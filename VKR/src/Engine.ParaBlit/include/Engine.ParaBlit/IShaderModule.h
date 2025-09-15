#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	struct ShaderModuleDesc
	{
		const char* m_byteCode = nullptr;
		u64 m_size = 0;
		u64 m_key[2];

		bool operator == (const ShaderModuleDesc& other) const;
	};

	class IShaderModuleCache
	{
	public:

		/*
		Description: Get or create a shader module with the provided desc object.
		Param:
			const ShaderModuleDesc& desc: The descriptor object containing shader module paramaters and a storage key.
		*/
		PARABLIT_INTERFACE ShaderModule GetModule(const ShaderModuleDesc& desc) = 0;

	private:


	};
}