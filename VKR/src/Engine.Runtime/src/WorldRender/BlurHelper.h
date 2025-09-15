#pragma once
#include "Engine.ParaBlit/ParaBlitDefs.h"

namespace PB
{
	class IRenderer;
	class ICommandContext;
	class ITexture;
	class IBufferObject;
}

namespace Eng
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

	enum class EBlurKernelSize : PB::u8
	{
		SIZE_4,
		SIZE_8,
		SIZE_16,
		SIZE_32,
		SIZE_64,
		SIZE_128
	};

	inline EBlurKernelSize BlurKernelSizeToEnum(PB::u64 size)
	{
		switch (size)
		{
		case 4:
			return EBlurKernelSize::SIZE_4;
		case 8:
			return EBlurKernelSize::SIZE_8;
		case 16:
			return EBlurKernelSize::SIZE_16;
		case 32:
			return EBlurKernelSize::SIZE_32;
		case 64:
			return EBlurKernelSize::SIZE_64;
		case 128:
			return EBlurKernelSize::SIZE_128;
		default:
			return EBlurKernelSize::SIZE_4;
		}
	}

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