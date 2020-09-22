#include "DescriptorRegistry.h"
#include "Device.h"
#include "PBUtil.h"
#include "ParaBlitDebug.h"

namespace PB
{
	inline VkDescriptorType GetDescTypeFromExpectedState(ETextureStateFlags state)
	{
		switch (state)
		{
		case PB_TEXTURE_STATE_SAMPLED:
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		default:
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
			break;
		}
	}

	void DescriptorRegistry::RegisterView(TextureViewData& viewData)
	{
		viewData.m_descriptorIndex = GetDescriptorIndex(EDescriptorType::DESCRIPTORTYPE_TEXTURE);

		// Assign this texture to it's respective descriptor slot.
		VkWriteDescriptorSet texWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
		texWrite.descriptorCount = 1;
		texWrite.descriptorType = GetDescTypeFromExpectedState((ETextureStateFlags)viewData.m_expectedState);
		texWrite.dstSet = m_masterSet;
		texWrite.dstBinding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_TEXTURE);
		texWrite.dstArrayElement = viewData.m_descriptorIndex;

		VkDescriptorImageInfo texImgInfo{};
		texImgInfo.imageView = viewData.m_view;
		texImgInfo.imageLayout = ConvertPBStateToImageLayout((ETextureState)viewData.m_expectedState);
		texImgInfo.sampler = VK_NULL_HANDLE;
		texWrite.pImageInfo = &texImgInfo;

		vkUpdateDescriptorSets(m_device->GetHandle(), 1, &texWrite, 0, nullptr);
	}

	void DescriptorRegistry::RegisterView(SamplerData& samplerData)
	{
		samplerData.m_descriptorIndex = GetDescriptorIndex(EDescriptorType::DESCRIPTORTYPE_SAMPLER);

		VkWriteDescriptorSet sampWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
		sampWrite.descriptorCount = 1;
		sampWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		sampWrite.dstSet = m_masterSet;
		sampWrite.dstBinding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_SAMPLER);
		sampWrite.dstArrayElement = samplerData.m_descriptorIndex;

		VkDescriptorImageInfo sampImgInfo{};
		sampImgInfo.imageView = VK_NULL_HANDLE;
		sampImgInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		sampImgInfo.sampler = samplerData.m_sampler;
		sampWrite.pImageInfo = &sampImgInfo;

		vkUpdateDescriptorSets(m_device->GetHandle(), 1, &sampWrite, 0, nullptr);
	}

	void DescriptorRegistry::FreeView(EDescriptorType type, u32 index)
	{
		if(index < DESCRIPTORINDEX_INVALID)
			m_descriptorStates[static_cast<u32>(type)].m_freeDescriptors.PushBack(index);
	}

	void DescriptorRegistry::Create(Device* device, VkDescriptorSet* outMasterSet, VkDescriptorSetLayout* outMasterSetLayout)
	{
		m_device = device;
		CreateDescriptorPool();
		AllocateDescriptorSets();
		*outMasterSet = m_masterSet;
		*outMasterSetLayout = m_masterSetLayout;
	}

	void DescriptorRegistry::Destroy()
	{
		if (m_desciptorPool)
		{
			vkDestroyDescriptorPool(m_device->GetHandle(), m_desciptorPool, nullptr);
			m_desciptorPool = VK_NULL_HANDLE;
		}

		if (m_masterSetLayout)
		{
			vkDestroyDescriptorSetLayout(m_device->GetHandle(), m_masterSetLayout, nullptr);
			m_masterSetLayout = VK_NULL_HANDLE;
		}
	}

	void DescriptorRegistry::CreateDescriptorPool()
	{
		CLib::Vector<VkDescriptorPoolSize, 2> poolSizes;

		VkDescriptorPoolSize& texPoolSize = poolSizes.PushBack();
		texPoolSize.descriptorCount = MaxTextureDescriptors;
		texPoolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

		VkDescriptorPoolSize& samplerPoolSize = poolSizes.PushBack();
		samplerPoolSize.descriptorCount = MaxSamplers;
		samplerPoolSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;

		VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
		poolInfo.flags = 0;
		poolInfo.poolSizeCount = poolSizes.Count();
		poolInfo.pPoolSizes = poolSizes.Data();
		poolInfo.maxSets = 1;

		PB_ERROR_CHECK(vkCreateDescriptorPool(m_device->GetHandle(), &poolInfo, nullptr, &m_desciptorPool));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_desciptorPool);
	}

	void DescriptorRegistry::AllocateDescriptorSets()
	{
		// We're using descriptor indexing instead of the traditional method of binding descriptor sets. As such, we only create one descriptor set with a massive amount of available descriptors.
		CLib::Vector<VkDescriptorSetLayoutBinding, 3> bindings;
		CLib::Vector<VkDescriptorBindingFlags, 3> bindingFlags;

		VkDescriptorSetLayoutBinding& texBinding = bindings.PushBack();
		texBinding.binding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_TEXTURE);
		texBinding.descriptorCount = MaxTextureDescriptors;
		texBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		texBinding.pImmutableSamplers = nullptr;
		texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindingFlags.PushBack() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
		
		VkDescriptorSetLayoutBinding& samplerBinding = bindings.PushBack();
		samplerBinding.binding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_SAMPLER);
		samplerBinding.descriptorCount = MaxSamplers;
		samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		samplerBinding.pImmutableSamplers = nullptr;
		samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindingFlags.PushBack() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

		// Binding flags for the bindings above, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT is required for descriptor indexing.
		VkDescriptorSetLayoutBindingFlagsCreateInfo masterBindingFlagsInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr };
		masterBindingFlagsInfo.bindingCount = bindingFlags.Count();
		masterBindingFlagsInfo.pBindingFlags = bindingFlags.Data();

		VkDescriptorSetLayoutCreateInfo masterSetLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
		masterSetLayoutInfo.flags = 0;
		masterSetLayoutInfo.bindingCount = bindings.Count();
		masterSetLayoutInfo.pBindings = bindings.Data();
		masterSetLayoutInfo.pNext = &masterBindingFlagsInfo;

		PB_ERROR_CHECK(vkCreateDescriptorSetLayout(m_device->GetHandle(), &masterSetLayoutInfo, nullptr, &m_masterSetLayout));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_masterSetLayout);

		VkDescriptorSetAllocateInfo masterSetAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
		masterSetAllocInfo.descriptorPool = m_desciptorPool;
		masterSetAllocInfo.descriptorSetCount = 1;
		masterSetAllocInfo.pSetLayouts = &m_masterSetLayout;

		PB_ERROR_CHECK(vkAllocateDescriptorSets(m_device->GetHandle(), &masterSetAllocInfo, &m_masterSet));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_masterSet);
	}
}