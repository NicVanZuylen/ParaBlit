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
}

struct GLFWwindow;

class Input;

class ObjectDispatchList;
class VertexPool;
class DrawBatch;
class RenderGraph;

class GBufferPass;
class DeferredLightingPass;

class ClientPlayground
{
public:

	ClientPlayground(PB::IRenderer* renderer, CLib::Allocator* allocator);
	~ClientPlayground();

	void Update(GLFWwindow* window, Input* input, float deltaTime, float elapsedTime);

private:

	// -------------------------------------------------------------------------
	// Helper Functions
	inline void InitResources();
	inline void DestroyResources();

	inline RenderGraph* CreateRenderGraph();

	inline void SetupDrawBatch();

	inline PB::Pipeline GetGBufferDrawBatchPipeline();
	inline PB::BindingLayout GetGBufferDrawBatchBindings(PB::UniformBufferView& mvpView);
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
	GBufferPass* m_gBufferPass = nullptr;
	DeferredLightingPass* m_deferredLightingPass = nullptr;
	// -------------------------------------------------------------------------

	// -------------------------------------------------------------------------
	// Resources
	PB::IBufferObject* m_mvpBuffer = nullptr;

	PBClient::Mesh* m_paintMesh = nullptr;
	PBClient::Mesh* m_detailsMesh = nullptr;
	PBClient::Mesh* m_glassMesh = nullptr;
	PBClient::Mesh* m_planeMesh = nullptr;

	PBClient::Texture* m_paintTextures[4]{};
	PBClient::Texture* m_detailsTextures[4]{};
	PBClient::Texture* m_glassTextures[4]{};
	PBClient::Texture* m_metalTextures[2]{};

	PB::ITexture* m_solidWhiteTexture = nullptr;
	PB::ITexture* m_solidBlackTexture = nullptr;
	PB::ITexture* m_flatNormalTexture = nullptr;

	PB::ResourceView m_paintViews[4]{};
	PB::ResourceView m_detailsViews[4]{};
	PB::ResourceView m_glassViews[4]{};
	PB::ResourceView m_colorSampler = 0;

	PBClient::Shader* m_vertShader = nullptr;
	PBClient::Shader* m_fragShader = nullptr;

	ObjectDispatchList* m_geoDispatchList = nullptr;
	VertexPool* m_vertexPool = nullptr;
	DrawBatch* m_drawBatch = nullptr;

	PB::u32 m_firstInstanceHandles[3]{};

	// -------------------------------------------------------------------------
};

