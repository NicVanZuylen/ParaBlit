#pragma once
#include "ParaBlitDefs.h"

namespace PB
{
	namespace Util
	{
		inline ETextureFormat FormatToSRGB(ETextureFormat format)
		{
			switch (format) 
			{
			case ETextureFormat::R8_UNORM:
				return ETextureFormat::R8_SRGB;
			case ETextureFormat::R8G8_UNORM:
				return ETextureFormat::R8G8_SRGB;
			case ETextureFormat::R8G8B8_UNORM:
				return ETextureFormat::R8G8B8_SRGB;
			case ETextureFormat::R8G8B8A8_UNORM:
				return ETextureFormat::R8G8B8A8_SRGB;
			case ETextureFormat::B8G8R8A8_UNORM:
				return ETextureFormat::B8G8R8A8_SRGB;
			default:
				return format;
			};
		}

		inline ETextureFormat FormatToUnorm(ETextureFormat format)
		{
			switch (format) 
			{
			case ETextureFormat::R8_SRGB:
				return ETextureFormat::R8_UNORM;
			case ETextureFormat::R8G8_SRGB:
				return ETextureFormat::R8G8_UNORM;
			case ETextureFormat::R8G8B8_SRGB:
				return ETextureFormat::R8G8B8_UNORM;
			case ETextureFormat::R8G8B8A8_SRGB:
				return ETextureFormat::R8G8B8A8_UNORM;
			case ETextureFormat::B8G8R8A8_SRGB:
				return ETextureFormat::B8G8R8A8_UNORM;
			default:
				return format;
			};
		}
	}
}