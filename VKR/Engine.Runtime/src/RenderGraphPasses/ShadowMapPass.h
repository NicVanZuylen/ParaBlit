#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Mesh.h"
#include "Resource/Texture.h"
#include "Resource/Shader.h"
#include "World/ObjectDispatcher.h"
#include "World/Camera.h"

namespace Eng
{
	class DrawBatch;
	class RenderGraphBuilder;
	class BatchDispatcher;
	class RenderBoundingVolumeHierarchy;
	class DebugLinePass;

	class ShadowMapPass : public RenderGraphBehaviour
	{
	public:

		struct CreateDesc
		{
			float m_frustrumSectionNear;
			float m_frustrumSectionFar;
			float m_softShadowPenumbraDistance;
			float m_shadowBiasMultiplier;
			uint32_t m_shadowmapResolution;
			uint32_t m_cascadeIndex;
		};

		ShadowMapPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

		~ShadowMapPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution);

		void SetOutputTexture(PB::ITexture* tex);

		void SetCamera(const Camera* camera, const RenderBoundingVolumeHierarchy* rbvh, glm::vec3 shadowDirection);

		void Update();

		PB::GraphicsPipelineDesc GetBasePipelineDesc(uint32_t shadowMapResolution) const;

		PB::BindingLayout GetDrawBatchBindings();

		PB::UniformBufferView GetShadowConstantsView();

		const Camera::CameraFrustrum& GetCascadeFrustrum() const { return m_shadowCascadeFrustrum; }

	private:

		struct ShadowConstants
		{
			glm::mat4 m_viewProjectionMatrix;
			glm::vec4 m_shadowFrustrumPlanes[6];
			glm::vec4 m_shadowViewDirection;
			glm::vec2 m_cascadeRange;
			float m_shadowPenumbraDistance;
			float m_shadowBiasMultiplier;
		} m_localShadowConstants{};

		PB::Pipeline m_shadowPipeline = 0;
		BatchDispatcher* m_batchDispatcher = nullptr;
		PB::IBufferObject* m_shadowViewBuffer = nullptr;
		PB::UniformBufferView m_shadowConstantsView = 0;
		PB::UniformBufferView m_viewPlanesView = nullptr;
		PB::BindingLayout m_batchBindings{};
		bool m_shadowConstantsRequireUpdate = false;

		PB::ITexture* m_outputTexture = nullptr;
		const Camera* m_camera = nullptr;
		Camera::CameraFrustrum m_shadowCascadeFrustrum;
		const RenderBoundingVolumeHierarchy* m_rbvh = nullptr;

		float m_frustrumSectionNear;
		float m_frustrumSectionFar;
		float m_softShadowPenumbraDistance;
		float m_shadowBiasMultiplier;
		uint32_t m_shadowmapResolution;
		uint32_t m_cascadeIndex;
	};
};