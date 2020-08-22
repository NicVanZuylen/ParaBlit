#pragma once
#include "ParaBlitDefs.h"
#include "ParaBlitInterface.h"

namespace PB
{
	class ITexture;
	class IRenderer;
	class IBufferObject;

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