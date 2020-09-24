#pragma once
#include "IShaderModule.h"
#include "ParaBlitDebug.h"

#include <unordered_map>

namespace PB
{
	class Device;

	struct ShaderModuleDescHasher
	{
		size_t operator()(const ShaderModuleDesc& desc) const;
	};

	class ShaderCache : public IShaderModuleCache
	{
	public:

		void Init(Device* device);

		void Destroy();

		PARABLIT_API ShaderModule GetModule(const ShaderModuleDesc& desc) override;

	private:

		ShaderModule CreateModule(const ShaderModuleDesc& desc);

		Device* m_device = nullptr;
		std::unordered_map<ShaderModuleDesc, ShaderModule, ShaderModuleDescHasher> m_moduleCache;
	};
}