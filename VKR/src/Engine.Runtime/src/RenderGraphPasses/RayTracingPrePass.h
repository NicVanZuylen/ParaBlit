#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Engine.ParaBlit/IAccelerationStructure.h"
#include "Engine.Math/Vectors.h"

namespace Eng
{
	class Texture;
	class RenderGraphBuilder;
	class EntityHierarchy;

	struct RaytracingWorldConstants
	{
		Math::Vector3f sunDirection = Math::Vector3f(0.0f, -1.0f, 0.0f);
		float sunDistance = 1000.0f;
		Math::Vector3f sunColor = Math::Vector3f(1.0f);
		float sunIntensity = 1.0f;
		float sunRadius = 100.0f;
		float randSeed = 0.0f;
		PB::u32 checkerboardIndex = 0;
		PB::u32 blueNoiseTextureLayerIndex = 0;
	};

	class RayTracingPrePass : public RenderGraphBehaviour
	{
	public:

		struct CreateDesc
		{
			EntityHierarchy* hierarchyToDraw = nullptr;
			PB::UniformBufferView viewConstView = nullptr;
			PB::UniformBufferView viewPlanesView = nullptr;
			PB::u32 initialInstanceCount = 256;
			PB::u32 instanceExpansionRate = 256;
		};

		RayTracingPrePass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

		~RayTracingPrePass();

		void Update(const float& deltaTime);

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution);

		void SetSkyboxTexture(Eng::Texture* skyboxTexture) { m_skyboxTexture = skyboxTexture; };

		RaytracingWorldConstants& GetSetWorldConstants() { return m_worldConstants; }
		PB::IBufferObject* GetSetWorldConstantsBuffer() const { return m_worldConstantsBuffer; };

		const PB::IAccelerationStructure* const* GetTLAS() const { return &m_topLevelAS; };

		PB::IBufferObject* const* GetInstanceIndexBuffer() const { return &m_asInstanceIndexBuffer; };

	private:

		void ResizeInstanceBuffer();

		CreateDesc m_desc;
		PB::ASGeometryDesc m_drawPoolGeometryDesc;
		PB::u32 m_maxInstanceCount = 0;
		PB::u32 m_instanceCount = 0;
		PB::u32 m_renderWidth = 0;
		PB::u32 m_renderHeight = 0;
		PB::IBufferObject* m_asInstanceBuffer = nullptr;
		PB::IBufferObject* m_asInstanceIndexBuffer = nullptr;
		PB::IBufferObject* m_worldConstantsBuffer = nullptr;
		PB::IAccelerationStructure* m_topLevelAS = nullptr;
		PB::Pipeline m_raytracingExamplePipeline = 0;

		RaytracingWorldConstants m_worldConstants{};
		float m_noiseLayerIncrementDelay = 0.0156f;
		float m_currentNoiseLayerIncrementDelay = m_noiseLayerIncrementDelay;

		Eng::Texture* m_skyboxTexture = nullptr;
	};
};