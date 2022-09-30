#pragma once
#include "bc7Enc/bc7enc.h"

#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace LightningBC7
{
	using RGBATexel = uint32_t;

	inline size_t CalcBC7TextureSizeBytes(uint32_t widthPixels, uint32_t heightPixels)
	{
		return ((widthPixels / 4) * (heightPixels / 4)) * 16;
	}

	// Compress a grid of texel blocks from firstBlockW/H to firstBlockW/H + blockCountW/H.
	void CompressJob(uint8_t* outputBlocks, const RGBATexel* inputTexels, uint32_t firstBlockW, uint32_t firstBlockH, uint32_t blockCountW, uint32_t blockCountH, uint32_t inputRowPitch)
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
			encoderThreads.emplace_back(CompressJob, outputBlocks, inputRGBATexels, 0, i * blockRowsPerThread, blockCountW, blockRowsPerThread, inputRowPitch);
		}

		// Wait for encoder threads to finish.
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			encoderThreads[i].join();
		}
		encoderThreads.clear();
	}
};