#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ISwapChain.h"
#include "Engine.ParaBlit/IImGUIModule.h"

#include "RenderGraph/RenderGraph.h"

#include "Entity/Component/Camera.h"

#include "Engine.Math/Vectors.h"

#include "Engine.Control/IDataClass.h"

#include "Resource/Texture.h"

namespace Eng
{
	class RenderGraph;
	class FontTexture;
	class EntityHierarchy;
	class DebugLinePass;
	class TextRenderPass;
	class EditorMain;

	class GameRenderer
	{
	public:

		GameRenderer() = default;

		~GameRenderer() {};

		void Init(PB::IRenderer* renderer, CLib::Allocator* allocator, EntityHierarchy* hierarchy, TObjectPtr<Camera> camera, EditorMain* editor = nullptr);

		void InitResources();

		void DestroyResources();

		void CreateWorldRenderGraph(RenderGraph* graph);

		void EndFrame(float deltaTime);

		void UpdateResolution(uint32_t width, uint32_t height);

		Math::Vector2u GetWorldRenderResolution() { return m_worldRenderResolution; }

		void SetCamera(TObjectPtr<Camera> camera);

		DebugLinePass* GetDebugLinePass() { return m_debugLinePass; }

		TextRenderPass* GetTextPass() { return m_textPass; }

		const PB::ImGuiTextureData& GetWorldRenderOutputData() { return m_worldRenderOutputData; }

	private:

		// -------------------------------------------------------------------------
		// Buffer Structures
		struct ViewConstantsBuffer
		{
			Matrix4 m_viewProj;
			Matrix4 m_viewProjLastFrame;
			Matrix4 m_view;
			Matrix4 m_proj;
			Matrix4 m_invView;
			Matrix4 m_invProj;
			Vector4f m_camPos;
			Vector4f m_camPosLastFrame;
			float m_aspectRatio;
			float m_tanHalfFOV;
			float m_pad[2];
		};

		struct FrustrumPlanesBuffer
		{
			Vector4f m_planes[6];
			Vector3f m_camPos;
			uint32_t m_isOrthographic = 0;
		};
		// -------------------------------------------------------------------------

		// -------------------------------------------------------------------------
		// Renderer handles
		
		PB::IRenderer* m_renderer = nullptr;
		PB::ISwapChain* m_swapchain = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		EntityHierarchy* m_hierarchy = nullptr;
		EditorMain* m_editor = nullptr;

		// -------------------------------------------------------------------------

		// -------------------------------------------------------------------------
		// Configuration

		Math::Vector2u m_worldRenderResolution = Math::Vector2u(1280.0f, 720.0f);

		// -------------------------------------------------------------------------

		// -------------------------------------------------------------------------
		// Render Graph
		static constexpr const uint32_t ShadowmapResolution = 2048;
		static constexpr const uint32_t ShadowCascadeCount = 5;

		class RayTracingPrePass* m_rtPrePass = nullptr;
		class PathTracingMainPass* m_pathTracingMainPass = nullptr;
		class ShadowMapPass* m_shadowmapPass[ShadowCascadeCount]{};
		class GBufferPass* m_gBufferPass = nullptr;
		class ShadowAccumPass* m_shadowAccumPass = nullptr;
		class ShadowBlurPass* m_shadowBlurPass = nullptr;
		class ReflectionBlurPass* m_reflectionBlurPass = nullptr;
		class DeferredLightingPass* m_deferredLightingPass = nullptr;
		class AmbientOcclusionPass* m_ambientOcclusionPass = nullptr;
		class AOBlurPass* m_aoBlurPass = nullptr;
		class BloomExtractionPass* m_bloomExtractionPass = nullptr;
		class BloomBlurPass* m_bloomBlurPass = nullptr;
		DebugLinePass* m_debugLinePass = nullptr;
		TextRenderPass* m_textPass = nullptr;

		RenderGraph m_mainRenderGraph{};
		// -------------------------------------------------------------------------

		// -------------------------------------------------------------------------
		// Resources

		TObjectPtr<Camera> m_camera;
		Matrix4 m_viewProjLastFrame;

		PB::ImGuiTextureData m_worldRenderOutputData{};
		PB::ITexture* m_worldRenderOutput = nullptr;

		PB::IBufferObject* m_mvpBuffer = nullptr;
		PB::IBufferObject* m_frustrumPlanesBuffer = nullptr;

		Eng::Texture* m_noiseTextureArray = nullptr;
		Eng::Texture* m_hdrSkyTexture = nullptr;
		Eng::Texture* m_skyIrradianceMap = nullptr;
		Eng::Texture* m_skyPrefilterMap = nullptr;

		float m_shadowCascadeSectionRanges[ShadowCascadeCount * 2]
		{
			0.0f,
			5.0f,
			5.0f,
			15.0f,
			15.0f,
			50.0f,
			50.0f,
			160.0f,
			160.0f,
			300.0f
		};

		static constexpr float m_shadowCascadeBiasValues[ShadowCascadeCount]
		{
			0.05f,
			0.1f,
			0.3f,
			0.5f,
			1.0f
		};

		// -------------------------------------------------------------------------
	};
}