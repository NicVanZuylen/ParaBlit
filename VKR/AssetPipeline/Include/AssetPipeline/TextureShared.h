#pragma once
#include <cstdint>

namespace AssetPipeline
{
	struct TextureMetadata
	{
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint8_t m_mipCount = 1;
		uint8_t m_arraySize = 1;
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
			return (m_width >> mipLevel) * (m_height >> mipLevel) * sizeof(float) * 2;
		}

		uint32_t m_mipmapAlignedSizes[ConvolutionMipmapCount]{};
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_arrayLayerSize = 0;
		EConvolutedMapType m_mapType = EConvolutedMapType::IRRADIANCE;
		uint8_t m_mipCount = 0;
		uint8_t m_pad = 0;
	};
}