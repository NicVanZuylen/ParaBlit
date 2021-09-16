#pragma once
#include "ParaBlitDefs.h"

namespace PB
{
	class IRenderer;
	class ICommandContext;
	class ITexture;
	class IBufferObject;
}

namespace PBClient
{
	struct BlurImageParams
	{
		PB::ITexture* m_src;
		PB::ITexture* m_buffer0;
		PB::ITexture* m_buffer1;
		PB::u32 m_srcMip;
		PB::u32 m_buffer0Mip;
		PB::u32 m_buffer1Mip;
		PB::ETextureFormat m_imageFormat;
	};

	class BlurHelper
	{
	public:

		BlurHelper() = default;

		void Init(PB::IRenderer* renderer, PB::u32 kernelSize);

		~BlurHelper();

		void Encode(PB::ICommandContext* cmdContext, PB::Uint2 dstResolution, const BlurImageParams& imageParams);

	private:

		struct BlurConstants
		{
			float m_guassianNormPart;
			float m_pad0[3];
		};

		PB::IRenderer* m_renderer = nullptr;
		PB::ShaderModule m_verticalPassModule = 0;
		PB::ShaderModule m_horizontalPassModule = 0;
		PB::IBufferObject* m_blurConstantsBuffer = nullptr;
		PB::ResourceView m_blurSampler = 0;
		PB::u32 m_kernelSize = 1;
	};
};