#pragma once
#include "IRenderer.h"
#include "ICommandContext.h"
#include "DescriptorRegistry.h"
#include "IBufferObject.h"
#include "ITexture.h"
#include "IBufferObject.h"

#include <unordered_map>

namespace PB
{
	class Device;

	struct TextureViewDescHasher
	{
		size_t operator()(const TextureViewDesc& desc) const;				// Unordered map hashing operator.
	};

	struct BufferViewDescHasher
	{
		size_t operator()(const BufferViewDesc& desc) const;
	};

	struct SamplerDescHasher
	{
		size_t operator()(const SamplerDesc& desc) const;
	};

	class ViewCache
	{
	public:

		ViewCache();

		~ViewCache();

		void Init(Device* device, VkDescriptorSet* outMasterSet, VkDescriptorSetLayout* outMasterSetLayout);

		void Destroy();

		ResourceView GetTextureView(const TextureViewDesc& desc);

		RenderTargetView GetRenderTargetView(const TextureViewDesc& desc);

		void DestroyTextureView(const TextureViewDesc& desc);

		UniformBufferView GetUniformBufferView(const BufferViewDesc& desc);

		void DestroyUniformBufferView(const BufferViewDesc& desc);

		ResourceView GetSSBOBufferView(const BufferViewDesc& desc);

		void DestroySSBOBufferView(const BufferViewDesc& desc);

		ResourceView GetSampler(const SamplerDesc& desc);

	private:

		inline TextureViewData CreateTextureView(const TextureViewDesc& desc);

		inline UBOViewData CreateUniformBufferView(const BufferViewDesc& desc);

		inline SSBOViewData CreateStorageBufferView(const BufferViewDesc& desc);

		inline SamplerData CreateSampler(const SamplerDesc& desc);

		Device* m_device = nullptr;
		std::unordered_map<TextureViewDesc, TextureViewData, TextureViewDescHasher> m_texViewCache;
		std::unordered_map<BufferViewDesc, UBOViewData, BufferViewDescHasher> m_uboViewCache;
		std::unordered_map<BufferViewDesc, SSBOViewData, BufferViewDescHasher> m_ssboViewCache;
		std::unordered_map<SamplerDesc, SamplerData, SamplerDescHasher> m_samplerCache;
		u32 m_nextTextureUniqueId = 0;
		DescriptorRegistry m_descriptorRegistry;
	};
}

