#pragma once
#include "ITextureViewCache.h"
#include "ICommandContext.h"
#include "DescriptorRegistry.h"
#include "IBufferObject.h"
#include "ITexture.h"
#include "IBufferObject.h"

#include "vulkan/vulkan.h"

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

		void Init(Device* device);

		void Destroy();

		PARABLIT_API TextureView GetTextureView(const TextureViewDesc& desc);

		void DestroyTextureView(const TextureViewDesc& desc);

		BufferView GetBufferView(const BufferViewDesc& desc);

		void DestroyBufferView(const BufferViewDesc& desc);

		Sampler GetSampler(const SamplerDesc& desc);

	private:

		inline TextureViewData CreateTextureView(const TextureViewDesc& desc);

		inline BufferViewData CreateBufferView(const BufferViewDesc& desc);

		inline SamplerData CreateSampler(const SamplerDesc& desc);

		Device* m_device = nullptr;
		std::unordered_map<TextureViewDesc, TextureViewData, TextureViewDescHasher> m_texViewCache;
		std::unordered_map<BufferViewDesc, BufferViewData, BufferViewDescHasher> m_bufViewCache;
		std::unordered_map<SamplerDesc, SamplerData, SamplerDescHasher> m_samplerCache;
		DescriptorRegistry m_descriptorRegistry;
	};
}

