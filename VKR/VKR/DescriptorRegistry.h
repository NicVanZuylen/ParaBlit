#pragma once
#include "ParaBlitDefs.h"
#include "ParaBlitDebug.h"
#include "CLib/Vector.h"


namespace PB
{
	enum class EDescriptorType
	{
		DESCRIPTORTYPE_TEXTURE,
		DESCRIPTORTYPE_SAMPLER,
		DESCRIPTORTYPE_STORAGE_BUFFER,
		DESCRIPTORTYPE_STORAGE_IMAGE,

		DESCRIPTORTYPE_COUNT
	};

	enum : u32
	{
		DESCRIPTORINDEX_INVALID = ~u32(0)
	};

	class Device;

	struct ViewData
	{
		u32 m_descriptorIndex = DESCRIPTORINDEX_INVALID;
	};

	struct TextureViewData : public ViewData
	{
		ETextureState m_expectedState = ETextureState::NONE;
		VkImageView m_view = VK_NULL_HANDLE;
	};

	struct UBOViewData
	{
		VkBuffer m_buffer = VK_NULL_HANDLE;
		u32 m_offset = 0;
		u32 m_size = ~0u;
	};

	struct SamplerData : public ViewData
	{
		VkSampler m_sampler = VK_NULL_HANDLE;
	};

	struct SSBOViewData : public ViewData
	{
		u32 m_offset = 0;
		u32 m_size = ~0u;
		VkBuffer m_buffer = VK_NULL_HANDLE;
	};

	struct StorageImageViewData : public ViewData
	{
		VkImageView m_view = VK_NULL_HANDLE;
	};

	class DescriptorRegistry
	{
	public:

		void Create(Device* device, VkDescriptorSet* outMasterSet, VkDescriptorSetLayout* outMasterSetLayout);

		void Destroy();

		void RegisterView(TextureViewData& viewData);
		void RegisterView(SamplerData& samplerData);
		void RegisterView(SSBOViewData& ssboData);
		void RegisterView(StorageImageViewData& storageImageData);

		void FreeView(EDescriptorType type, u32 index);

	private:

		inline void CreateDescriptorPool();

		inline void AllocateDescriptorSets();

		inline u32 GetDescriptorIndex(const EDescriptorType& type)
		{
			if (m_descriptorStates[static_cast<u32>(type)].m_freeDescriptors.Count() > 0)
				return m_descriptorStates[static_cast<u32>(type)].m_freeDescriptors.PopBack();
			return m_descriptorStates[static_cast<u32>(type)].m_usedDescriptorCount++;
		}

		static constexpr const u32 MaxDescriptors = 0xFFFFFF;
		static constexpr const u32 MaxTextureDescriptors = 40000;
		static constexpr const u32 MaxSamplers = 40000;
		static constexpr const u32 MaxSSBOs = 40000;
		static constexpr const u32 MaxStorageImages = 40000;

		struct DescriptorState
		{
			CLib::Vector<u32> m_freeDescriptors;
			u32 m_usedDescriptorCount = 0;
		};
		DescriptorState m_descriptorStates[static_cast<u64>(EDescriptorType::DESCRIPTORTYPE_COUNT)] = {};

		// Contains uniform buffers.
		VkDescriptorSetLayout m_uboSetLayout = VK_NULL_HANDLE;

		// Contains all Texture, Sampler etc. descriptors.
		VkDescriptorSetLayout m_masterSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet m_masterSet = VK_NULL_HANDLE;

		Device* m_device = nullptr;
		VkDescriptorPool m_desciptorPool = VK_NULL_HANDLE;
	};
}