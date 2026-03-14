#pragma once
#include "Engine.ParaBlit/ParaBlitDefs.h"
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Texture.h"
#include "Resource/Shader.h"
#include "Entity/Component/Camera.h"

#define SHADOW_MAP_PASS_DEBUG_DRAW_CASCADES 0

namespace Eng
{
	class DrawBatch;
	class RenderGraphBuilder;
	class BatchDispatcher;
	class RenderBoundingVolumeHierarchy;
	class DebugLinePass;
	class EntityHierarchy;

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

		void SetCamera(const Camera* camera, PB::UniformBufferView cameraViewPlanesView, EntityHierarchy* hierarchyToDraw, const RenderBoundingVolumeHierarchy* rbvh, Vector3f shadowDirection);

		void Update();

		void DebugDrawCascadeVolumes(class DebugLinePass* linePass);

		PB::GraphicsPipelineDesc GetBasePipelineDesc(uint32_t shadowMapResolution) const;

		PB::BindingLayout GetDrawBatchBindings();

		PB::UniformBufferView GetShadowConstantsView();

		const Camera::CameraFrustrum& GetCascadeFrustrum() const { return m_shadowCascadeFrustrum; }

	private:

		struct ShadowConstants
		{
			Matrix4 m_viewProjectionMatrix;
			Vector4f m_shadowViewDirection;
			Vector2f m_cascadeRange;
			float m_shadowPenumbraDistance;
			float m_shadowBiasMultiplier;
		} m_localShadowConstants{};

		struct ShadowPlaneConstants
		{
			Vector4f m_shadowFrustrumPlanes[6];
			Vector3f m_cameraOrigin;
			uint32_t m_isOrthographic = 1;
		} m_localShadowPlaneConstants{};

		PB::Pipeline m_shadowPipeline = 0;
		BatchDispatcher* m_batchDispatcher = nullptr;
		PB::IBufferObject* m_shadowViewBuffer = nullptr;
		PB::IBufferObject* m_shadowPlanesBuffer = nullptr;
		PB::UniformBufferView m_shadowConstantsView = 0;
		PB::UniformBufferView m_viewPlanesView = 0;
		PB::UniformBufferView m_mainCamViewPlanesView = 0;
		bool m_shadowConstantsRequireUpdate = false;

		PB::ITexture* m_outputTexture = nullptr;
		const Camera* m_camera = nullptr;
		Camera::CameraFrustrum m_shadowCascadeFrustrum;

		EntityHierarchy* m_hierarchyToDraw = nullptr;
		const RenderBoundingVolumeHierarchy* m_rbvh = nullptr;

		float m_frustrumSectionNear;
		float m_frustrumSectionFar;
		float m_softShadowPenumbraDistance;
		float m_shadowBiasMultiplier;
		uint32_t m_shadowmapResolution;
		uint32_t m_cascadeIndex;

		// Debug
#if SHADOW_MAP_PASS_DEBUG_DRAW_CASCADES
		bool m_debugFrustrumIsSet = false;
		Camera::CameraFrustrum m_debugCamFrustrum;
		Camera::CameraFrustrum m_debugCascadeFrustrum;
		Vector3f m_debugEye;
		Vector3f m_debugCascadeCenter;
#endif
	};
};