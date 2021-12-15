#pragma once
#include "IRenderer.h"
#include "ISwapChain.h"

#include "Camera.h"

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

class ShadowMapPass;
class GBufferPass;
class ShadowAccumPass;
class DeferredLightingPass;
class AmbientOcclusionPass;
class BloomPass;

class ClientPlayground
{
public:

	ClientPlayground(PB::IRenderer* renderer, CLib::Allocator* allocator);
	~ClientPlayground();

	void Update(GLFWwindow* window, Input* input, float deltaTime, float elapsedTime);

	void UpdateResolution(uint32_t width, uint32_t height);

private:

	static constexpr const uint32_t ShadowmapResolution = 2048;

	// -------------------------------------------------------------------------
	// Helper Functions
	inline void InitResources();
	inline void DestroyResources();

	inline RenderGraph* CreateRenderGraph();

	inline void SetupDrawBatch();

	inline PB::Pipeline GetGBufferDrawBatchPipeline();
	inline PB::BindingLayout GetGBufferDrawBatchBindings(PB::UniformBufferView& mvpView);
	inline PB::Pipeline GetShadowDrawBatchPipeline();
	// -------------------------------------------------------------------------

	// -------------------------------------------------------------------------
	// Buffer Structures
	struct MVPBuffer
	{
		glm::mat4 m_model;
		glm::mat4 m_view;
		glm::mat4 m_proj;
		glm::mat4 m_invView;
		glm::mat4 m_invProj;
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
	ShadowMapPass* m_shadowmapPass = nullptr;
	GBufferPass* m_gBufferPass = nullptr;
	ShadowAccumPass* m_shadowAccumPass = nullptr;
	DeferredLightingPass* m_deferredLightingPass = nullptr;
	AmbientOcclusionPass* m_ambientOcclusionPass = nullptr;
	BloomPass* m_bloomPass = nullptr;
	// -------------------------------------------------------------------------

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

	PB::ITexture* m_solidWhiteTexture = nullptr;
	PB::ITexture* m_solidBlackTexture = nullptr;
	PB::ITexture* m_flatNormalTexture = nullptr;

	PBClient::FontTexture* m_fontTexture = nullptr;

	PB::ResourceView m_paintViews[5]{};
	PB::ResourceView m_detailsViews[5]{};
	PB::ResourceView m_glassViews[5]{};
	PB::ResourceView m_colorSampler = 0;

	PBClient::Shader* m_shadowVertShader = nullptr;
	PBClient::Shader* m_vertShader = nullptr;
	PBClient::Shader* m_fragShader = nullptr;

	ObjectDispatchList* m_geoShadowDispatchList = nullptr;
	ObjectDispatchList* m_geoDispatchList = nullptr;
	VertexPool* m_vertexPool = nullptr;
	DrawBatch* m_drawBatch = nullptr;

	PB::u32 m_firstInstanceHandles[3]{};

	// -------------------------------------------------------------------------
};

