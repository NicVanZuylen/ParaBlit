#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ISwapChain.h"

#include "World/Camera.h"

namespace CLib
{
	class Allocator;
}

namespace PBClient
{
	class Texture;
	class Mesh;
	class Shader;
	class FontTexture;
}

struct GLFWwindow;

class Input;

class ObjectDispatchList;
class VertexPool;
class DrawBatch;
class RenderGraph;
class Material;

class ClientPlayground
{
public:

	ClientPlayground(PB::IRenderer* renderer, CLib::Allocator* allocator);
	~ClientPlayground();

	void Update(GLFWwindow* window, Input* input, float deltaTime, float elapsedTime, float stallTime, bool updateMetrics);

	void UpdateResolution(uint32_t width, uint32_t height);

private:

	static constexpr const uint32_t ShadowmapResolution = 2048;
	static constexpr const uint32_t ShadowCascadeCount = 4;

	// -------------------------------------------------------------------------
	// Helper Functions
	inline void InitResources();
	inline void DestroyResources();

	inline RenderGraph* CreateRenderGraph();

	inline void SetupDrawBatch();

	inline PB::BindingLayout GetGBufferDrawBatchBindings(PB::UniformBufferView& mvpView);
	inline PB::Pipeline GetShadowDrawBatchPipeline();
	// -------------------------------------------------------------------------

	// -------------------------------------------------------------------------
	// Buffer Structures
	struct ViewConstantsBuffer
	{
		glm::mat4 m_viewProj;
		glm::mat4 m_view;
		glm::mat4 m_proj;
		glm::mat4 m_invView;
		glm::mat4 m_invProj;
		glm::vec4 m_mainFrustrumPlanes[6];
		glm::vec4 m_camPos;
		float m_aspectRatio;
		float m_tanHalfFOV;
		float m_pad[2];
	};
	// -------------------------------------------------------------------------

	// -------------------------------------------------------------------------
	// Essential
	PB::IRenderer* m_renderer = nullptr;
	PB::ISwapChain* m_swapchain = nullptr;
	CLib::Allocator* m_allocator = nullptr;
	RenderGraph* m_renderGraph = nullptr;
	Camera m_camera;
	// -------------------------------------------------------------------------

	// -------------------------------------------------------------------------
	// Render Graph
	class ShadowMapPass* m_shadowmapPass[ShadowCascadeCount]{};
	class GBufferPass* m_gBufferPass = nullptr;
	class ShadowAccumPass* m_shadowAccumPass = nullptr;
	class ShadowBlurPass* m_shadowBlurPass = nullptr;
	class DeferredLightingPass* m_deferredLightingPass = nullptr;
	class AmbientOcclusionPass* m_ambientOcclusionPass = nullptr;
	class AOBlurPass* m_aoBlurPass = nullptr;
	class BloomExtractionPass* m_bloomExtractionPass = nullptr;
	class BloomBlurPass* m_bloomBlurPass = nullptr;
	class DebugLinePass* m_debugLinePass = nullptr;
	class TextRenderPass* m_textPass = nullptr;
	// -------------------------------------------------------------------------

	class RenderBoundingVolumeHierarchy* m_renderHierarchy = nullptr;

	// -------------------------------------------------------------------------
	// Resources
	PB::IBufferObject* m_mvpBuffer = nullptr;

	PBClient::Mesh* m_paintMesh = nullptr;
	PBClient::Mesh* m_detailsMesh = nullptr;
	PBClient::Mesh* m_glassMesh = nullptr;
	PBClient::Mesh* m_planeMesh = nullptr;

	PBClient::Texture* m_paintTextures[4]{};
	PBClient::Texture* m_detailsTextures[5]{};
	PBClient::Texture* m_glassTextures[5]{};
	PBClient::Texture* m_metalTextures[2]{};
	PBClient::Texture* m_debugTextures[3]{};

	Material* m_spinnerMaterials[3]{};
	Material* m_planeMaterial = nullptr;
	Material* m_debugMaterial = nullptr;

	PBClient::Texture* m_hdrSkyTexture = nullptr;
	PBClient::Texture* m_skyIrradianceMap = nullptr;
	PBClient::Texture* m_skyPrefilterMap = nullptr;

	PB::ITexture* m_solidWhiteTexture = nullptr;
	PB::ITexture* m_solidBlackTexture = nullptr;
	PB::ITexture* m_flatNormalTexture = nullptr;

	PBClient::FontTexture* m_fontTexture = nullptr;
	void* m_cpuTimeText = nullptr;
	void* m_fpsText = nullptr;

	PB::ResourceView m_paintViews[5]{};
	PB::ResourceView m_detailsViews[5]{};
	PB::ResourceView m_glassViews[5]{};
	PB::ResourceView m_debugViews[3]{};
	PB::ResourceView m_colorSampler = 0;

	PBClient::Shader* m_shadowVertShader = nullptr;

	VertexPool* m_vertexPool = nullptr;

	float m_shadowCascadeSectionRanges[ShadowCascadeCount * 2]
	{
		0.0f,
		10.0f,
		10.0f,
		30.0f,
		30.0f,
		80.0f,
		80.0f,
		150.0f
	};

	uint32_t m_renderHierarchyDrawDebugDepth = 0;
	bool m_drawEntireRenderHierarchy = false;

	// -------------------------------------------------------------------------
};

