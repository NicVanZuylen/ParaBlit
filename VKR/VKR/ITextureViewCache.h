#pragma once
#include "ParaBlitDefs.h"
#include "ParaBlitInterface.h"

namespace PB
{
	class ITexture;
	class IRenderer;
	class IBufferObject;

	enum ESamplerFilter
	{
		PB_SAMPLER_FILTER_NEAREST,
		PB_SAMPLER_FILTER_BILINEAR,
	};

	enum ESamplerRepeatMode
	{
		PB_SAMPLER_REPEAT_REPEAT,
		PB_SAMPLER_REPEAT_MIRRORED_REPEAT,
		PB_SAMPLER_REPEAT_CLAMP,
	};

	struct SamplerDesc
	{
		ESamplerFilter m_filter = PB_SAMPLER_FILTER_BILINEAR;
		ESamplerFilter m_mipFilter = PB_SAMPLER_FILTER_BILINEAR;
		ESamplerRepeatMode m_repeatMode = PB_SAMPLER_REPEAT_REPEAT;
		float m_anisotropyLevels = 0.0f;

		bool operator == (const SamplerDesc& other) const;
	};

	class IViewCache
	{
	public:

		//PARABLIT_INTERFACE TextureView GetTextureView(const TextureViewDesc& desc) = 0;
		//
		//PARABLIT_INTERFACE BufferView GetBufferView(const BufferViewDesc& desc) = 0;
		//
		//PARABLIT_INTERFACE Sampler GetSampler(const SamplerDesc& desc) = 0;
	};
}