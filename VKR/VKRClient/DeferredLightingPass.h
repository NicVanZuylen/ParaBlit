#pragma once
#include "RenderGraphNode.h"
#include "Shader.h"
#include "Mesh.h"

namespace PBClient
{
	class Texture;
}

class RenderGraphBuilder;

class DeferredLightingPass : public RenderGraphBehaviour
{
public:

	DeferredLightingPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~DeferredLightingPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetMVPBuffer(PB::IBufferObject* buf);

	void SetDirectionalLight(uint32_t index, PB::Float4 direction, PB::Float4 color);

	void SetPointLight(uint32_t index, PB::Float4 position, PB::Float3 color, float radius);

	void SetSkyboxTexture(PBClient::Texture* skyboxTexture, uint32_t mipCount = 1);

private:

	struct LightingBuffer
	{
		struct PointLightingData
		{
			struct PointLight
			{
				PB::Float4 m_position;
				PB::Float3 m_color;
				float m_radius;
			} m_lights[512];
		} m_pointLights;

		struct DirectionalLightingData
		{
			struct DirectionalLight
			{
				PB::Float4 m_direction;
				PB::Float4 m_color;
			} m_lights[8];
			int32_t m_lightCount;
			float m_emissionIntensityScale;
		} m_directionalLights;
	} m_localLightingData{};

	PBClient::Mesh m_pointLightVolumeMesh;
	PB::u32 m_pointLightCount = 0;
	PB::u32 m_directionalLightCount = 0;
	PB::IBufferObject* m_mvpBuffer = nullptr;
	PB::IBufferObject* m_lightingBuffer = nullptr;
	PBClient::Texture* m_skyboxTexture = nullptr;
	uint32_t m_skyboxMipCount = 1;

	PB::UniformBufferView m_pointLightingView = nullptr;
	PB::UniformBufferView m_dirLightingView = nullptr;
	PB::Pipeline m_dirLightingPipeline = 0;
	PB::Pipeline m_pointLightingPipeline = 0;
	PBClient::Shader* m_screenQuadShader = nullptr;
	PBClient::Shader* m_defDirLightShader = nullptr;
	PBClient::Shader* m_pointLightVTXShader = nullptr;
	PBClient::Shader* m_pointLightShader = nullptr;

	PB::ResourceView m_gBufferSampler = 0;

	PB::IBufferObject* m_pointLightIndirectParamsBuffer = nullptr;
	PB::ICommandList* m_reusableCmdList = nullptr;

	LightingBuffer* m_mappedLightingBuffer = nullptr;
};

