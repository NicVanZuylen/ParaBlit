#pragma once
#include "RenderGraphNode.h"
#include "Shader.h"
#include "Mesh.h"

class DeferredLightingPass : public RenderGraphBehaviour
{
public:

	DeferredLightingPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~DeferredLightingPass();

	void OnPrePass(const RenderGraphInfo& info) override;

	void OnPassBegin(const RenderGraphInfo& info) override;

	void OnPostPass(const RenderGraphInfo& info) override;

	void SetOutputTexture(PB::ITexture* tex);

	void SetMVPBuffer(PB::IBufferObject* buf);

	void SetDirectionalLight(uint32_t index, PB::Float4 direction, PB::Float4 color);

	void SetPointLight(uint32_t index, PB::Float4 position, PB::Float3 color, float radius);

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
			int m_lightCount;
		} m_directionalLights;
	};

	PB::ITexture* m_outputTexture = nullptr;
	PBClient::Mesh m_pointLightVolumeMesh;
	PB::u32 m_pointLightCount = 0;
	PB::IBufferObject* m_mvpBuffer = nullptr;
	PB::IBufferObject* m_lightingBuffer = nullptr;

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

