#include "DescriptorRegistry.h"
#include "Device.h"
#include "PBUtil.h"
#include "ParaBlitDebug.h"

namespace PB
{
	inline VkDescriptorType GetDescTypeFromExpectedState(ETextureState state)
	{
		switch (state)
		{
		case ETextureState::SAMPLED:
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
		texWrite.descriptorType = GetDescTypeFromExpectedState(viewData.m_expectedState);
		texWrite.dstSet = m_masterSet;
		texWrite.dstBinding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_TEXTURE);
		texWrite.dstArrayElement = viewData.m_descriptorIndex;

		VkDescriptorImageInfo texImgInfo{};
		texImgInfo.imageView = viewData.m_view;
		texImgInfo.imageLayout = ConvertPBStateToImageLayout(viewData.m_expectedState);
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

	void DescriptorRegistry::RegisterView(SSBOViewData& ssboData)
	{
		ssboData.m_descriptorIndex = GetDescriptorIndex(EDescriptorType::DESCRIPTORTYPE_STORAGE_BUFFER);

		VkWriteDescriptorSet ssboWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
		ssboWrite.descriptorCount = 1;
		ssboWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ssboWrite.dstSet = m_masterSet;
		ssboWrite.dstBinding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_STORAGE_BUFFER);
		ssboWrite.dstArrayElement = ssboData.m_descriptorIndex;

		VkDescriptorBufferInfo ssboInfo{};
		ssboInfo.buffer = ssboData.m_buffer;
		ssboInfo.offset = ssboData.m_offset;
		ssboInfo.range = ssboData.m_size;
		ssboWrite.pBufferInfo = &ssboInfo;

		vkUpdateDescriptorSets(m_device->GetHandle(), 1, &ssboWrite, 0, nullptr);
	}

	void DescriptorRegistry::RegisterView(StorageImageViewData& storageImageData)
	{
		storageImageData.m_descriptorIndex = GetDescriptorIndex(EDescriptorType::DESCRIPTORTYPE_STORAGE_IMAGE);

		VkWriteDescriptorSet storageImgWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
		storageImgWrite.descriptorCount = 1;
		storageImgWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		storageImgWrite.dstSet = m_masterSet;
		storageImgWrite.dstBinding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_STORAGE_IMAGE);
		storageImgWrite.dstArrayElement = storageImageData.m_descriptorIndex;

		VkDescriptorImageInfo storageImageInfo;
		storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storageImageInfo.imageView = storageImageData.m_view;
		storageImageInfo.sampler = VK_NULL_HANDLE;
		storageImgWrite.pImageInfo = &storageImageInfo;

		vkUpdateDescriptorSets(m_device->GetHandle(), 1, &storageImgWrite, 0, nullptr);
	}

	void DescriptorRegistry::FreeView(EDescriptorType type, u32 index)
	{
		if (index < DESCRIPTORINDEX_INVALID)
		{
			m_descriptorStates[static_cast<u32>(type)].m_freeDescriptors.PushBack(index);

			// We need to write a null descriptor in place of the free descriptor, as the resource the free descriptor belongs to is assumed to be destroyed.
			VkWriteDescriptorSet nullDescriptorWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
			nullDescriptorWrite.descriptorCount = 1;
			nullDescriptorWrite.dstSet = m_masterSet;
			nullDescriptorWrite.dstBinding = static_cast<u32>(type);
			nullDescriptorWrite.dstArrayElement = index;

			switch (type)
			{
			case PB::EDescriptorType::DESCRIPTORTYPE_TEXTURE:
				nullDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_SAMPLER:
				nullDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_STORAGE_BUFFER:
				nullDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_STORAGE_IMAGE:
				nullDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_COUNT:
			default:
				PB_NOT_IMPLEMENTED;
				break;
			}

			switch (type)
			{
			case PB::EDescriptorType::DESCRIPTORTYPE_TEXTURE:
				nullDescriptorWrite.pImageInfo = &m_nullImageInfo;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_SAMPLER:
				nullDescriptorWrite.pImageInfo = &m_nullSamplerInfo;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_STORAGE_BUFFER:
				nullDescriptorWrite.pBufferInfo = &m_nullBufferInfo;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_STORAGE_IMAGE:
				nullDescriptorWrite.pImageInfo = &m_nullImageInfo;
				break;
			case PB::EDescriptorType::DESCRIPTORTYPE_COUNT:
			default:
				PB_NOT_IMPLEMENTED;
				break;
			}

			vkUpdateDescriptorSets(m_device->GetHandle(), 1, &nullDescriptorWrite, 0, nullptr);
		}
	}

	void DescriptorRegistry::Create(Device* device, VkDescriptorSet* outMasterSet, VkDescriptorSetLayout* outMasterSetLayout)
	{
		m_device = device;
		uint32_t graphicsQueueFamilyIndex = m_device->GetGraphicsQueueFamilyIndex();

		// Create null resources.
		VkBufferCreateInfo nullBufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
		nullBufferInfo.flags = 0;
		nullBufferInfo.pQueueFamilyIndices = &graphicsQueueFamilyIndex;
		nullBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		nullBufferInfo.size = 4;
		nullBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		PB_ERROR_CHECK(vkCreateBuffer(m_device->GetHandle(), &nullBufferInfo, nullptr, &m_nullBuffer));
		PB_BREAK_ON_ERROR;

		VkImageCreateInfo nullImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr };
		nullImageCreateInfo.flags = 0;
		nullImageCreateInfo.arrayLayers = 1;
		nullImageCreateInfo.mipLevels = 1;
		nullImageCreateInfo.extent = { 32, 32, 1 };
		nullImageCreateInfo.format = VK_FORMAT_R8_UINT;
		nullImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		nullImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		nullImageCreateInfo.pQueueFamilyIndices = &graphicsQueueFamilyIndex;
		nullImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		nullImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		nullImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		nullImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		PB_ERROR_CHECK(vkCreateImage(m_device->GetHandle(), &nullImageCreateInfo, nullptr, &m_nullImage));
		PB_BREAK_ON_ERROR;

		VkSamplerCreateInfo nullSamplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr };
		nullSamplerInfo.compareEnable = VK_FALSE;
		nullSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		nullSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		nullSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		nullSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		nullSamplerInfo.anisotropyEnable = VK_FALSE;
		nullSamplerInfo.maxAnisotropy = 1.0f;
		nullSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		nullSamplerInfo.minFilter = VK_FILTER_NEAREST;
		nullSamplerInfo.magFilter = VK_FILTER_NEAREST;
		nullSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		nullSamplerInfo.minLod = 0.0f;
		nullSamplerInfo.maxLod = 1.0f;
		nullSamplerInfo.mipLodBias = 0.0f;
		PB_ERROR_CHECK(vkCreateSampler(m_device->GetHandle(), &nullSamplerInfo, nullptr, &m_nullSampler));
		PB_BREAK_ON_ERROR;

		// Allocate memory for null resources.
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->GetHandle(), m_nullImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = m_device->FindMemoryTypeIndex(memRequirements.memoryTypeBits, EMemoryType::DEVICE_LOCAL);
		PB_ERROR_CHECK(vkAllocateMemory(m_device->GetHandle(), &allocInfo, nullptr, &m_nullResourceMemory));
		PB_BREAK_ON_ERROR;

		vkBindImageMemory(m_device->GetHandle(), m_nullImage, m_nullResourceMemory, 0);
		vkBindBufferMemory(m_device->GetHandle(), m_nullBuffer, m_nullResourceMemory, 0);

		// Create views
		VkImageSubresourceRange subresources;
		subresources.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresources.baseArrayLayer = 0;
		subresources.baseMipLevel = 0;
		subresources.layerCount = 1;
		subresources.levelCount = 1;

		VkImageViewCreateInfo sampledViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
		sampledViewInfo.flags = 0;
		sampledViewInfo.format = VK_FORMAT_R8_UINT;
		sampledViewInfo.image = m_nullImage;
		sampledViewInfo.subresourceRange = subresources;
		sampledViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vkCreateImageView(m_device->GetHandle(), &sampledViewInfo, nullptr, &m_nullImageView);

		// Set up null binding info.
		m_nullBufferInfo.buffer = m_nullBuffer;
		m_nullBufferInfo.offset = 0;
		m_nullBufferInfo.range = 4;

		m_nullImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		m_nullImageInfo.imageView = m_nullImageView;
		m_nullImageInfo.sampler = VK_NULL_HANDLE;

		m_nullSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		m_nullSamplerInfo.imageView = VK_NULL_HANDLE;
		m_nullSamplerInfo.sampler = m_nullSampler;

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

		if (m_nullImage)
		{
			vkDestroyImageView(m_device->GetHandle(), m_nullImageView, nullptr);
			vkDestroyImage(m_device->GetHandle(), m_nullImage, nullptr);
		}

		if (m_nullBuffer)
		{
			vkDestroyBuffer(m_device->GetHandle(), m_nullBuffer, nullptr);
		}

		if (m_nullSampler)
		{
			vkDestroySampler(m_device->GetHandle(), m_nullSampler, nullptr);
		}

		if (m_nullResourceMemory)
		{
			vkFreeMemory(m_device->GetHandle(), m_nullResourceMemory, nullptr);
		}
	}

	void DescriptorRegistry::CreateDescriptorPool()
	{
		CLib::Vector<VkDescriptorPoolSize, 2> poolSizes;

		PB_ASSERT_MSG(MaxTextureDescriptors < m_device->GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindSampledImages, "MaxTextureDescriptors count not supported by device.");
		PB_ASSERT_MSG(MaxSamplers < m_device->GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindSamplers, "MaxSamplers count not supported by device.");
		PB_ASSERT_MSG(MaxSSBOs < m_device->GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindStorageBuffers, "MaxSSBOs count not supported by device.");
		PB_ASSERT_MSG(MaxStorageImages < m_device->GetDescriptorIndexingProperties()->maxPerStageDescriptorUpdateAfterBindStorageImages, "MaxStorageImages count not supported by device.");

		VkDescriptorPoolSize& texPoolSize = poolSizes.PushBack();
		texPoolSize.descriptorCount = MaxTextureDescriptors;
		texPoolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

		VkDescriptorPoolSize& samplerPoolSize = poolSizes.PushBack();
		samplerPoolSize.descriptorCount = MaxSamplers;
		samplerPoolSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;

		VkDescriptorPoolSize& ssboPoolSize = poolSizes.PushBack();
		ssboPoolSize.descriptorCount = MaxSSBOs;
		ssboPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

		VkDescriptorPoolSize& storageImagePoolSize = poolSizes.PushBack();
		storageImagePoolSize.descriptorCount = MaxStorageImages;
		storageImagePoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
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

		// TODO: Should we really have one pair of descriptor sets to share across all shader stages? What is the performance impact of this?
		VkShaderStageFlags stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorBindingFlags commonBindingFlags
		{
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
		};

		VkDescriptorSetLayoutBinding& texBinding = bindings.PushBack();
		texBinding.binding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_TEXTURE);
		texBinding.descriptorCount = MaxTextureDescriptors;
		texBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		texBinding.pImmutableSamplers = nullptr;
		texBinding.stageFlags = stageFlags;
		bindingFlags.PushBack() = commonBindingFlags;
		
		VkDescriptorSetLayoutBinding& samplerBinding = bindings.PushBack();
		samplerBinding.binding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_SAMPLER);
		samplerBinding.descriptorCount = MaxSamplers;
		samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		samplerBinding.pImmutableSamplers = nullptr;
		samplerBinding.stageFlags = stageFlags;
		bindingFlags.PushBack() = commonBindingFlags;

		VkDescriptorSetLayoutBinding& ssboBinding = bindings.PushBack();
		ssboBinding.binding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_STORAGE_BUFFER);
		ssboBinding.descriptorCount = MaxSamplers;
		ssboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ssboBinding.pImmutableSamplers = nullptr;
		ssboBinding.stageFlags = stageFlags;
		bindingFlags.PushBack() = commonBindingFlags;

		VkDescriptorSetLayoutBinding& storageImgBinding = bindings.PushBack();
		storageImgBinding.binding = static_cast<u32>(EDescriptorType::DESCRIPTORTYPE_STORAGE_IMAGE);
		storageImgBinding.descriptorCount = MaxStorageImages;
		storageImgBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		storageImgBinding.pImmutableSamplers = nullptr;
		storageImgBinding.stageFlags = stageFlags;
		bindingFlags.PushBack() = commonBindingFlags;

		// Binding flags for the bindings above, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT is required for descriptor indexing.
		VkDescriptorSetLayoutBindingFlagsCreateInfo masterBindingFlagsInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr };
		masterBindingFlagsInfo.bindingCount = bindingFlags.Count();
		masterBindingFlagsInfo.pBindingFlags = bindingFlags.Data();

		VkDescriptorSetLayoutCreateInfo masterSetLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
		masterSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
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