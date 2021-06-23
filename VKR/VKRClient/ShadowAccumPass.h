#pragma once
#include "RenderGraphNode.h"
#include "Shader.h"

#pragma warning(push, 0)
#include "glm.hpp"
#pragma warning(pop)

class ShadowAccumPass : public RenderGraphBehaviour
{
public:

	ShadowAccumPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~ShadowAccumPass();

	void OnPrePass(const RenderGraphInfo& info) override;

	void OnPassBegin(const RenderGraphInfo& info) override;

	void OnPostPass(const RenderGraphInfo& info) override;

	void SetOutputTexture(PB::ITexture* tex);

	void SetMVPBuffer(PB::IBufferObject* buf);

	void SetSVBBuffer(PB::UniformBufferView view);

private:

	//static constexpr const uint32_t PCFDiskSampleCount = 16;
	static constexpr const uint32_t PCFRandomRotationTextureSize = 64;

	struct BlurConstants
	{
		float m_depthDiscontinuityThreshold;
		float m_normalDiscontinuityThreshold;
		float m_depthScaleFactor;
		float m_sigma;
		float m_guassianNormPart;
	};

	void GenerateRandomRotationTexture(glm::vec2* pixelValues);

	PB::ITexture* m_outputTexture = nullptr;
	PB::IBufferObject* m_mvpBuffer = nullptr;
	PB::IBufferObject* m_blurConstants = nullptr;
	PB::UniformBufferView m_svbView = nullptr;
	PB::UniformBufferView m_blurConstantsView = nullptr;

	PB::ITexture* m_randomRotationTexture = nullptr;
	PB::ResourceView m_randomRotationTextureView = 0;
	PB::ResourceView m_gBufferSampler = 0;
	PB::ResourceView m_shadowSampler = 0;
	PB::ResourceView m_randomRotationSampler = 0;
	PB::ResourceView m_blurImageSampler = 0;

	PB::ICommandList* m_reusableCmdListA = nullptr;
	PB::ICommandList* m_reusableCmdListB = nullptr;
};

