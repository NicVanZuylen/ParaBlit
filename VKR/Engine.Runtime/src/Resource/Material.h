#pragma once
#include "CLib/Vector.h"
#include "Engine.ParaBlit/ITexture.h"
#include "Engine.AssetEncoder/AssetDatabaseReader.h"

namespace Eng
{
	class Material
	{
	public:

		Material(PB::Pipeline pipeline, AssetEncoder::AssetID* textureIDs, uint32_t textureCount, PB::ResourceView sampler);

		~Material() = default;

		uint32_t GetTextureCount() const { return m_textureIDs.Count(); }

		AssetEncoder::AssetID* GetTextureIDs() { return m_textureIDs.Data(); }

		PB::ResourceView GetSampler() { return m_sampler; };

		PB::Pipeline GetPipeline() { return m_pipeline; }

	private:

		CLib::Vector<AssetEncoder::AssetID, 8> m_textureIDs;
		PB::ResourceView m_sampler;
		PB::Pipeline m_pipeline;
	};
};