#pragma once
#include <cstdint>

#include "ParaBlitDefs.h"

namespace AssetPipeline
{
	struct TextureMetadata
	{
		PB::u16 m_width = 0;
		PB::u16 m_height = 0;
		PB::ETextureFormat m_format;
		PB::u8 m_mipCount = 1;
		PB::u8 m_arraySize = 1;
		bool m_isHdr = false;
		uint8_t m_pad = 0;
	};

	static constexpr uint32_t SkyboxDimensions = 1024;
	static constexpr uint32_t IrradianceMapDimensions = 32;
	static constexpr uint32_t PrefilterMapDimensions = 512;
	static constexpr uint32_t ConvolutionMipmapCount = 6;

	enum class EConvolutedMapType : uint16_t
	{
		SKY,
		IRRADIANCE,
		PREFILTER
	};

	struct EnvironmentMapMetadata
	{
		uint32_t GetMipLevelSize(uint32_t mipLevel)
		{
			if (m_compressed == true)
			{
				uint32_t blockCountW = (m_width >> mipLevel) / 4;
				uint32_t blockCountH = (m_height >> mipLevel) / 4;
				return blockCountW * blockCountH * 16;
			}
			else
			{
				return (m_width >> mipLevel) * (m_height >> mipLevel) * sizeof(float) * 2;
			}
		}

		uint32_t m_mipmapAlignedSizes[ConvolutionMipmapCount]{};
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_arrayLayerSize = 0;
		EConvolutedMapType m_mapType = EConvolutedMapType::IRRADIANCE;
		uint8_t m_mipCount = 0;
		bool m_compressed = false;
	};
}