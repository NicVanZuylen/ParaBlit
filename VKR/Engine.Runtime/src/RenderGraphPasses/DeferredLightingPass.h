#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Shader.h"
#include "Resource/Mesh.h"

namespace Eng
{
	class Texture;

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

		void SetSkyboxTexture(Eng::Texture* skyboxTexture, Eng::Texture* irradianceMap, Eng::Texture* prefilterMap);

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

		void GenSpecBDRFLut(PB::ICommandContext* cmdContext);

		Eng::Mesh m_pointLightVolumeMesh;
		PB::u32 m_pointLightCount = 0;
		PB::u32 m_directionalLightCount = 0;
		PB::IBufferObject* m_mvpBuffer = nullptr;
		PB::IBufferObject* m_lightingBuffer = nullptr;
		Eng::Texture* m_skyboxTexture = nullptr;
		Eng::Texture* m_irradianceMap = nullptr;
		Eng::Texture* m_prefilterEnvMap = nullptr;
		PB::ITexture* m_specBDRFLut = nullptr;

		PB::UniformBufferView m_pointLightingView = nullptr;
		PB::UniformBufferView m_dirLightingView = nullptr;
		PB::Pipeline m_dirLightingPipeline = 0;
		PB::Pipeline m_pointLightingPipeline = 0;
		Eng::Shader* m_screenQuadShader = nullptr;
		Eng::Shader* m_defDirLightShader = nullptr;
		Eng::Shader* m_pointLightVTXShader = nullptr;
		Eng::Shader* m_pointLightShader = nullptr;

		PB::ResourceView m_gBufferSampler = 0;

		PB::IBufferObject* m_pointLightIndirectParamsBuffer = nullptr;
		PB::ICommandList* m_reusableCmdList = nullptr;

		LightingBuffer* m_mappedLightingBuffer = nullptr;
	};

};