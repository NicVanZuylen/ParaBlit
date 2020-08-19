#pragma once
#include "ParaBlitDefs.h"
#include "CLib/Vector.h"

#include <vulkan/vulkan.h>

namespace PB
{
	enum EDescriptorType
	{
		DESCRIPTORTYPE_UNIFORM_BUFFER,
		DESCRIPTORTYPE_TEXTURE,
		DESCRIPTORTYPE_SAMPLER,

		DESCRIPTORTYPE_COUNT
	};

	class Device;

	struct TextureViewData
	{
		VkImageView m_view = VK_NULL_HANDLE;
		u32 m_descriptorIndex;
		u32 m_expectedState;
	};

	struct BufferViewData
	{
		VkBuffer m_buffer = VK_NULL_HANDLE;
		u32 m_descriptorIndex = ~0u;
		u32 m_offset = 0;
		u32 m_size = ~0u;
	};

	struct SamplerData
	{
		VkSampler m_sampler = VK_NULL_HANDLE;
		u32 m_descriptorIndex = ~0u;
	};

	class DescriptorRegistry
	{
	public:

		void Create(Device* device);

		void Destroy();

		void RegisterView(BufferViewData& viewData);
		void RegisterView(TextureViewData& viewData);
		void RegisterView(SamplerData& samplerData);

		void FreeView(EDescriptorType type, u32 index);

	private:

		inline void CreateDescriptorPool();

		inline void AllocateDescriptorSets();

		inline u32 GetDescriptorIndex(const EDescriptorType& type)
		{
			if (m_descriptorStates[type].m_freeDescriptors.Count() > 0)
				return m_descriptorStates[type].m_freeDescriptors.PopBack();
			return m_descriptorStates[type].m_usedDescriptorCount++;
		}

		static constexpr const u32 MaxDescriptors = 0xFFFFFF;
		static constexpr const u32 MaxUBODescriptors = 40000;
		static constexpr const u32 MaxTextureDescriptors = 40000;
		static constexpr const u32 MaxSamplers = 40000;

		struct DescriptorState
		{
			CLib::Vector<u32> m_freeDescriptors;
			u32 m_usedDescriptorCount = 0;
		};
		DescriptorState m_descriptorStates[DESCRIPTORTYPE_COUNT] = {};

		VkDescriptorSetLayout m_masterSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet m_masterSet = VK_NULL_HANDLE;

		Device* m_device;
		VkDescriptorPool m_desciptorPool = VK_NULL_HANDLE;
	};
}