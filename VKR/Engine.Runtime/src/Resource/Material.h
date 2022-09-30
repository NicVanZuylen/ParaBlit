#pragma once
#include "CLib/Vector.h"
#include "Engine.ParaBlit/ITexture.h"

class Material
{
public:

	Material(PB::Pipeline pipeline, PB::ITexture** textures, uint32_t textureCount, PB::ResourceView sampler);

	~Material() = default;

	PB::ResourceView GetSampler() { return m_sampler; };

	PB::Pipeline GetPipeline() { return m_pipeline; }

	PB::BindingLayout GetBindings();

private:

	CLib::Vector<PB::ITexture*, 8> m_textures;
	CLib::Vector<PB::ResourceView, 8> m_textureViews;
	PB::ResourceView m_sampler;
	PB::Pipeline m_pipeline;
};