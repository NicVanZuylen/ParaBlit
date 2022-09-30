#include "ShaderModule.h"
#include "ParaBlitDebug.h"
#include "Device.h"
#include "MurmurHash/MurmurHash3.h"

namespace PB
{
	size_t ShaderModuleDescHasher::operator()(const ShaderModuleDesc& desc) const
	{
		return MurmurHash3_x64_64(desc.m_key, static_cast<int>(desc.m_keySize), 0);
	}

	bool ShaderModuleDesc::operator == (const ShaderModuleDesc& other) const
	{
		return MurmurHash3_x64_64(m_key, static_cast<int>(m_keySize), 0) == MurmurHash3_x64_64(other.m_key, static_cast<int>(other.m_keySize), 0);
	}

	void ShaderCache::Init(Device* device)
	{
		m_device = device;
	}

	void ShaderCache::Destroy()
	{
		for (auto& module : m_moduleCache)
			vkDestroyShaderModule(m_device->GetHandle(), reinterpret_cast<VkShaderModule>(module.second), nullptr);
	}

	ShaderModule ShaderCache::GetModule(const ShaderModuleDesc& desc)
	{
		auto it = m_moduleCache.find(desc);
		if (it == m_moduleCache.end())
		{
			if (!desc.m_key || desc.m_size == 0)
			{
				return 0;
			}

			ShaderModule newModule = CreateModule(desc);
			m_moduleCache[desc] = newModule;
			return newModule;
		}
		else
			return m_moduleCache[desc];
	}

	ShaderModule ShaderCache::CreateModule(const ShaderModuleDesc& desc)
	{
		PB_ASSERT_MSG(desc.m_byteCode, "Module code not provided.");
		PB_ASSERT_MSG(desc.m_size > 0, "Zero-sized code block provided for shader module creation.");

		VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr };
		moduleInfo.flags = 0;
		moduleInfo.codeSize = desc.m_size;
		moduleInfo.pCode = reinterpret_cast<const uint32_t*>(desc.m_byteCode);

		ShaderModule newModule;
		PB_ERROR_CHECK(vkCreateShaderModule(m_device->GetHandle(), &moduleInfo, nullptr, reinterpret_cast<VkShaderModule*>(&newModule)));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newModule);
		return newModule;
	}
}
