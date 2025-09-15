#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Engine.ParaBlit/IAccelerationStructure.h"
#include "Engine.Math/Vectors.h"

namespace Eng
{
	class Texture;
	class RenderGraphBuilder;

	class PathTracingMainPass : public RenderGraphBehaviour
	{
	public:

		struct CreateDesc
		{
			const PB::IAccelerationStructure* const* tlas = nullptr;
			PB::IBufferObject* const* tlasInstanceIndexBuffer = nullptr;
			PB::IBufferObject* worldConstantsBuffer = nullptr;
			PB::UniformBufferView viewConstView = nullptr;
			Texture* noiseTexturesArray = nullptr;
			Texture* skyboxTexture = nullptr;
			uint32_t shadowRaysPerPixel = 1; // Higher values reduce shadow noise, but have a great performance cost.
			bool useCameraRays = false;	// When set to true: Improves quality of small shadow details at a significant performance cost.
		};

		PathTracingMainPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

		~PathTracingMainPass();

		void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

		void AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution);

	private:

		CreateDesc m_desc;
		PB::u32 m_renderWidth = 0;
		PB::u32 m_renderHeight = 0;
		PB::u32 m_currentAccumTexIndex = 0;
		PB::Pipeline m_shadowRaytracingPipeline = 0;
		PB::ITexture* m_shadowAccumTextures[2] = {};
		PB::ITexture* m_reflectionAccumTextures[2] = {};
		PB::ResourceView m_noiseTextureView = 0;
		PB::ResourceView m_skyboxView = 0;
		PB::ResourceView m_skySampler = 0;

		uint8_t m_normalGBufferIndex = 0;
		uint8_t m_specAndRoughGBufferIndex = 0;
		uint8_t m_depthBufferIndex = 0;
		uint8_t m_outShadowMaskIndex = 0;
		uint8_t m_outPenumbraMaskIndex = 0;
		uint8_t m_motionVectorsIndex = 0;
		uint8_t m_outReflectionIndex = 0;
	};
};