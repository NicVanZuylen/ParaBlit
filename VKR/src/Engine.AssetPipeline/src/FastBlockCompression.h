#pragma once
#include "bc7enc/bc7enc.h"
#include "cmp_core.h"

#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>
#include <cassert>

namespace FastBlockCompression
{
	using RGBATexel = uint32_t;

	inline size_t CalcBC7TextureSizeBytes(uint32_t widthPixels, uint32_t heightPixels)
	{
		return ((widthPixels / 4) * (heightPixels / 4)) * 16;
	}

	// Compress a grid of texel blocks from firstBlockW/H to firstBlockW/H + blockCountW/H.
	void CompressBC7Job(uint8_t* outputBlocks, const RGBATexel* inputTexels, uint32_t firstBlockW, uint32_t firstBlockH, uint32_t blockCountW, uint32_t blockCountH, uint32_t inputRowPitch)
	{
		bc7enc_compress_block_params params;
		bc7enc_compress_block_params_init(&params);
		bc7enc_compress_block_params_init_perceptual_weights(&params);

		uint32_t blockEndW = firstBlockW + blockCountW;
		uint32_t blockEndH = firstBlockH + blockCountH;

		const RGBATexel* inputRGBATexels = reinterpret_cast<const RGBATexel*>(inputTexels);
		RGBATexel blockTexels[16];
		for (uint32_t y = firstBlockH; y < blockEndH; ++y)
		{
			uint32_t blockRowOffset = y * inputRowPitch * 4;
			for (uint32_t x = firstBlockW; x < blockEndW; ++x)
			{
				uint32_t inputXOffset = blockRowOffset + (x * 4);

				// Populate input texel memory 4 rows of 4 texels.
				for (uint32_t blockRow = 0; blockRow < 4; ++blockRow)
				{
					uint32_t blockTexelRowOffset = blockRow * inputRowPitch;
					uint32_t offsetTexels = inputXOffset + blockTexelRowOffset;
					memcpy(&blockTexels[blockRow * 4], &inputRGBATexels[offsetTexels], sizeof(RGBATexel) * 4);
				}

				bc7enc_compress_block(&outputBlocks[((y * blockCountW) + x) * 16], blockTexels, &params);
			}
		}
	}

	inline void CompressBC7(const void* inputTexels, uint8_t* outputBlocks, uint32_t widthPixels, uint32_t heightPixels)
	{
		assert(widthPixels % 4 == 0);
		assert(heightPixels % 4 == 0);

		bc7enc_compress_block_init();

		const uint32_t inputRowPitch = widthPixels;
		const uint32_t blockCountW = widthPixels / 4;
		const uint32_t blockCountH = heightPixels / 4;
		const uint32_t blockCount = blockCountW * blockCountH;

		const RGBATexel* inputRGBATexels = reinterpret_cast<const RGBATexel*>(inputTexels);

		// Spin up threads to encode rows of texel blocks. Row work is divided evenly into each thread.
		constexpr uint32_t MaxThreads = 64;
		std::vector<std::thread> encoderThreads;
		encoderThreads.reserve(MaxThreads);

		// Dispatch work.
		uint32_t threadCount = std::min<uint32_t>(blockCountH, MaxThreads);
		uint32_t blockRowsPerThread = blockCountH / threadCount;
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			encoderThreads.emplace_back(CompressBC7Job, outputBlocks, inputRGBATexels, 0, i * blockRowsPerThread, blockCountW, blockRowsPerThread, inputRowPitch);
		}

		// Wait for encoder threads to finish.
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			encoderThreads[i].join();
		}
		encoderThreads.clear();
	}

	inline size_t CalcBC6TextureSizeBytes(uint32_t widthPixels, uint32_t heightPixels)
	{
		constexpr uint64_t BlockSizeBytes = 16;
		constexpr uint64_t BlockTexelWidth = 4;
		constexpr uint64_t BlockTexelHeight = 4;

		const uint64_t blockCountW = widthPixels / BlockTexelWidth;
		const uint64_t blockCountH = heightPixels / BlockTexelHeight;

		return size_t(blockCountW * blockCountH * BlockSizeBytes);
	}

	void CompressBC6Job
	(
		uint8_t* outputBlocks, 
		const uint8_t* srcData, 
		uint32_t firstBlockW, 
		uint32_t firstBlockH, 
		uint32_t blockCountW, 
		uint32_t blockCountH, 
		uint32_t inputRowPitchShorts,
		void* bc6Options
	)
	{
		constexpr uint32_t BlockSizeBytes = 16;
		constexpr uint32_t BlockTexelWidth = 4;
		constexpr uint32_t BlockTexelHeight = 4;
		constexpr uint32_t ShortsPerTexel = 4;
		uint32_t blockEndW = firstBlockW + blockCountW;
		uint32_t blockEndH = firstBlockH + blockCountH;

		for (uint32_t y = firstBlockH; y < blockEndH; ++y)
		{
			uint32_t inputRowOffset = y * inputRowPitchShorts * BlockTexelHeight * sizeof(uint16_t);
			uint32_t outputRowOffset = y * blockCountW * BlockSizeBytes;
			for (uint32_t x = firstBlockW; x < blockEndW; ++x)
			{
				uint32_t inputColOffset = x * BlockTexelWidth * ShortsPerTexel * sizeof(uint16_t);
				uint32_t outputColOffset = x * BlockSizeBytes;

				const uint8_t* srcPos = srcData + inputRowOffset + inputColOffset;
				uint8_t* blockOutPos = outputBlocks + outputRowOffset + outputColOffset;

				CompressBlockBC6((const uint16_t*)srcPos, inputRowPitchShorts, 4 /* col stride */, blockOutPos, bc6Options);
			}
		}
	}

	inline void CompressBC6(const uint8_t* srcData, uint8_t* outputBlocks, uint32_t widthPixels, uint32_t heightPixels)
	{
		constexpr uint32_t BlockSizeBytes = 16;
		constexpr uint32_t BlockTexelWidth = 4;
		constexpr uint32_t BlockTexelHeight = 4;
		constexpr uint32_t ShortsPerTexel = 4;

		const uint32_t inputRowPitchShorts = widthPixels * ShortsPerTexel; // Multiplied by channel count per texel.
		const uint32_t blockCountW = widthPixels / BlockTexelWidth;
		const uint32_t blockCountH = heightPixels / BlockTexelHeight;

		void* bc6Options = nullptr;
		CreateOptionsBC6(&bc6Options);

		SetQualityBC6(bc6Options, 0.75f); // 0.75 Seems to be roughly the highest quality that encodes in a sane amount of time.
		SetSignedBC6(bc6Options, false);

		// Spin up threads to encode rows of texel blocks. Row work is divided evenly into each thread.
		constexpr uint32_t MaxThreads = 64;
		std::vector<std::thread> encoderThreads;
		encoderThreads.reserve(MaxThreads);

		// Dispatch work.
		uint32_t threadCount = std::min<uint32_t>(blockCountH, MaxThreads);
		uint32_t blockRowsPerThread = blockCountH / threadCount;
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			encoderThreads.emplace_back(CompressBC6Job, outputBlocks, srcData, 0, i * blockRowsPerThread, blockCountW, blockRowsPerThread, inputRowPitchShorts, bc6Options);
		}

		// Wait for encoder threads to finish.
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			encoderThreads[i].join();
		}
		encoderThreads.clear();

		DestroyOptionsBC6(bc6Options);
		bc6Options = nullptr;
	}
};