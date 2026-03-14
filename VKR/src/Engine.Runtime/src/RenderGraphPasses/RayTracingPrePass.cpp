#include "RayTracingPrePass.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/Shader.h"
#include "Entity/EntityHierarchy.h"
#include "Resource/Texture.h"

#include <random>
#include <chrono>

namespace Eng
{
	RayTracingPrePass::RayTracingPrePass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc) : RenderGraphBehaviour(renderer, allocator)
	{
		m_desc = desc;
		m_renderer = renderer;
		m_allocator = allocator;

		AssetPipeline:

		enum class ERTPermutation
		{
			STAGE,
			SHADER_GROUP,
			PERMUTATION_END
		};
		AssetEncoder::ShaderPermutationTable<ERTPermutation> permTable{};

		PB::RayTracingPipelineDesc rtExamplePipelineDesc{};
		permTable.SetPermutation(ERTPermutation::STAGE, AssetEncoder::ERTShaderStagePermutation::RAYGEN);
		rtExamplePipelineDesc.rayGenShaderModule = Eng::Shader(m_renderer, "Shaders/GLSL/rt_raytrace_example", permTable.GetKey(), m_allocator, true).GetModule();

		// Group 0
		{
			permTable.SetPermutation(ERTPermutation::STAGE, AssetEncoder::ERTShaderStagePermutation::MISS);
			rtExamplePipelineDesc.missShaderModules[0] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_raytrace_example", permTable.GetKey(), m_allocator, true).GetModule();
			permTable.SetPermutation(ERTPermutation::STAGE, AssetEncoder::ERTShaderStagePermutation::CLOSESTHIT);
			rtExamplePipelineDesc.closestHitShaderModules[0] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_raytrace_example", permTable.GetKey(), m_allocator, true).GetModule();
		}

		// Group 1
		permTable.SetPermutation(ERTPermutation::SHADER_GROUP, 1);
		{
			permTable.SetPermutation(ERTPermutation::STAGE, AssetEncoder::ERTShaderStagePermutation::MISS);
			rtExamplePipelineDesc.missShaderModules[1] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_raytrace_example", permTable.GetKey(), m_allocator, true).GetModule();
		}

		rtExamplePipelineDesc.maxPipelineRecursionCount = 2;
		m_raytracingExamplePipeline = m_renderer->GetPipelineCache()->GetPipeline(rtExamplePipelineDesc);

		PB::BufferObjectDesc worldConstantsDesc{};
		worldConstantsDesc.m_name = "RaytracingPrePass::worldConstants";
		worldConstantsDesc.m_bufferSize = sizeof(RaytracingWorldConstants);
		worldConstantsDesc.m_options = 0;
		worldConstantsDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM;
		m_worldConstantsBuffer = m_renderer->AllocateBuffer(worldConstantsDesc);
	}

	RayTracingPrePass::~RayTracingPrePass()
	{
		if (m_asInstanceBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_asInstanceBuffer);
			m_asInstanceBuffer = nullptr;
		}

		if (m_asInstanceIndexBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_asInstanceIndexBuffer);
			m_asInstanceIndexBuffer = nullptr;
		}

		if (m_topLevelAS != nullptr)
		{
			m_renderer->FreeAccelerationStructure(m_topLevelAS);
			m_topLevelAS = nullptr;
		}

		if (m_worldConstantsBuffer)
		{
			m_renderer->FreeBuffer(m_worldConstantsBuffer);
			m_worldConstantsBuffer = nullptr;
		}
	}

	void RayTracingPrePass::Update(const float& deltaTime)
	{
		m_currentNoiseLayerIncrementDelay -= 1.0f * deltaTime;

		if (m_currentNoiseLayerIncrementDelay <= 0.0f)
		{
			m_worldConstants.blueNoiseTextureLayerIndex = ++m_worldConstants.blueNoiseTextureLayerIndex % 64;
			m_currentNoiseLayerIncrementDelay = m_noiseLayerIncrementDelay;
		}
	}

	void RayTracingPrePass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("RayTracingPrePass::buildAccelerationStructures", { 1.0f, 1.0f, 1.0f, 1.0f });

		auto* hierarchy = m_desc.hierarchyToDraw;

		uint32_t oldMaxInstanceCount = m_maxInstanceCount;
		m_instanceCount = 0;

		DynamicDrawPool& pool = hierarchy->GetDynamicDrawPool();
		StaticObjectRenderer& staticObjectRenderer = hierarchy->GetStaticObjectRenderer();

		m_maxInstanceCount = pool.GetBatchCount() * DrawBatch::MaxObjects;
		m_maxInstanceCount += staticObjectRenderer.GetDrawPoolSize() * DrawBatch::MaxObjects;

		if (m_maxInstanceCount > oldMaxInstanceCount)
		{
			ResizeInstanceBuffer();
		}

		PB::u32 dynamicInstanceCount = 0;
		PB::u32 staticInstanceCount = 0;

		// Dynamic
		{
			pool.UpdateTLASInstances(info.m_commandContext, m_asInstanceBuffer, m_asInstanceIndexBuffer, 0, m_desc.viewPlanesView, dynamicInstanceCount);
			m_maxInstanceCount += pool.GetBatchCount() * DrawBatch::MaxObjects;
			m_instanceCount += dynamicInstanceCount;
		}

		// Barrier between updates.
		{
			PB::BufferMemoryBarrier barrier(m_asInstanceBuffer, PB::EMemoryBarrierType::COMPUTE_SHADER_WRITE_TO_COMPUTE_SHADER_READ);
			info.m_commandContext->CmdBufferBarrier(&barrier, 1);
		}

		// Static
		{
			staticObjectRenderer.UpdateTLASInstances(info.m_commandContext, m_asInstanceBuffer, m_asInstanceIndexBuffer, dynamicInstanceCount, m_desc.viewPlanesView, staticInstanceCount);
			m_maxInstanceCount += staticObjectRenderer.GetDrawPoolSize() * DrawBatch::MaxObjects;
			m_instanceCount += staticInstanceCount;
		}

		// Update -> Build barrier
		{
			PB::BufferMemoryBarrier barrier(m_asInstanceBuffer, PB::EMemoryBarrierType::COMPUTE_SHADER_WRITE_TO_ACCELERATION_STRUCTURE_BUILD);
			info.m_commandContext->CmdBufferBarrier(&barrier, 1);
		}

		// World constants update.
		{
			std::default_random_engine randEngine(std::chrono::system_clock::now().time_since_epoch().count());
			std::uniform_real_distribution distribution(0.0f, 1.0f);

			m_worldConstants.randSeed = distribution(randEngine);
			m_worldConstants.checkerboardIndex = m_worldConstants.checkerboardIndex == 0 ? 1 : 0;

			RaytracingWorldConstants* constants = reinterpret_cast<RaytracingWorldConstants*>(m_worldConstantsBuffer->BeginPopulate());
			*constants = m_worldConstants;
			m_worldConstantsBuffer->EndPopulate();
		}

		info.m_commandContext->CmdBuildAccelerationStructure(m_topLevelAS, &m_instanceCount);

		info.m_commandContext->CmdEndLastLabel();
	}

	void RayTracingPrePass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		/*

		info.m_commandContext->CmdBeginLabel("RayTracingPrePass::exampleTrace", { 1.0f, 1.0f, 1.0f, 1.0f });

		const PB::IAccelerationStructure* asArr = m_topLevelAS;
		info.m_commandContext->CmdBuildAccelerationStructureToTraceRaysBarrier(&asArr, 1);

		info.m_commandContext->CmdBindPipeline(m_raytracingExamplePipeline);

		PB::TextureViewDesc outputViewDesc{};
		outputViewDesc.m_texture = transientTextures[0];
		outputViewDesc.m_expectedState = PB::ETextureState::STORAGE;
		outputViewDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;

		PB::TextureViewDesc skyboxViewDesc{};
		skyboxViewDesc.m_texture = m_skyboxTexture->GetTexture();
		skyboxViewDesc.m_expectedState = PB::ETextureState::SAMPLED;
		skyboxViewDesc.m_format = m_skyboxTexture->IsCompressed() ? PB::ETextureFormat::BC6H_RGB_U16F : PB::ETextureFormat::R16G16B16A16_FLOAT;
		skyboxViewDesc.m_type = PB::ETextureViewType::VIEW_TYPE_CUBE;
		skyboxViewDesc.m_subresources.m_arrayCount = 1;
		skyboxViewDesc.m_subresources.m_mipCount = 1;

		PB::ResourceView skyboxView = m_skyboxTexture->GetTexture()->GetView(skyboxViewDesc);

		PB::SamplerDesc skySamplerDesc;
		skySamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
		skySamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		skySamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
		skySamplerDesc.maxLod = 1.0f;
		PB::ResourceView skySampler = m_renderer->GetSampler(skySamplerDesc);

		PB::UniformBufferView uniformViews[] = { m_desc.viewConstView, m_worldConstantsBuffer->GetViewAsUniformBuffer() };
		PB::ResourceView resources[] =
		{
			m_asInstanceIndexBuffer->GetViewAsStorageBuffer(),
			Mesh::GetMeshLibraryView(),
			skyboxView,
			skySampler,
			transientTextures[0]->GetViewAsStorageImage(outputViewDesc)
		};

		PB::BindingLayout bindings{};
		bindings.m_uniformBufferCount = PB_ARRAY_LENGTH(uniformViews);
		bindings.m_uniformBuffers = uniformViews;
		bindings.m_resourceCount = PB_ARRAY_LENGTH(resources);
		bindings.m_resourceViews = resources;

		info.m_commandContext->CmdBindResources(bindings);
		info.m_commandContext->CmdBindAccelerationStructure(m_topLevelAS);

		info.m_commandContext->CmdTraceRays(m_renderWidth, m_renderHeight, 1);

		info.m_commandContext->CmdEndLastLabel();

		*/
	}

	void RayTracingPrePass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{

	}

	void RayTracingPrePass::AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;
		nodeDesc.m_computeOnlyPass = true;
		nodeDesc.m_renderWidth = 1;
		nodeDesc.m_renderHeight = 1;

		m_renderWidth = targetResolution.x;
		m_renderHeight = targetResolution.y;

		builder->AddNode(nodeDesc);
	}

	void RayTracingPrePass::ResizeInstanceBuffer()
	{
		if (m_asInstanceBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_asInstanceBuffer);
			m_asInstanceBuffer = nullptr;
		}

		if (m_asInstanceIndexBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_asInstanceIndexBuffer);
			m_asInstanceIndexBuffer = nullptr;
		}

		if (m_topLevelAS != nullptr)
		{
			m_renderer->FreeAccelerationStructure(m_topLevelAS);
			m_topLevelAS = nullptr;
		}

		{
			PB::BufferObjectDesc instanceBufferDesc{};
			instanceBufferDesc.m_name = "RayTracingPrePass::instanceBuffer";
			instanceBufferDesc.m_usage = PB::EBufferUsage::MEMORY_ADDRESS_ACCESS | PB::EBufferUsage::STORAGE;
			instanceBufferDesc.m_options = 0;
			instanceBufferDesc.m_bufferSize = m_maxInstanceCount * sizeof(PB::AccelerationStructureInstance);
			m_asInstanceBuffer = m_renderer->AllocateBuffer(instanceBufferDesc);
		}

		{
			PB::BufferObjectDesc instanceIndexBufferDesc{};
			instanceIndexBufferDesc.m_name = "RayTracingPrePass::instanceIndexBuffer";
			instanceIndexBufferDesc.m_usage = PB::EBufferUsage::STORAGE;
			instanceIndexBufferDesc.m_options = 0;
			instanceIndexBufferDesc.m_bufferSize = m_maxInstanceCount * sizeof(PB::u32);
			m_asInstanceIndexBuffer = m_renderer->AllocateBuffer(instanceIndexBufferDesc);
		}

		auto& instances = m_drawPoolGeometryDesc.instances;
		instances.deviceInstanceData = m_asInstanceBuffer;
		instances.instanceDataOffsetBytes = 0;
		instances.useHostInstanceData = false;
		instances.maxInstanceCount = m_maxInstanceCount;

		PB::AccelerationStructureDesc asDesc{};
		asDesc.type = PB::AccelerationStructureType::TOP_LEVEL;
		asDesc.geometryInputs = &m_drawPoolGeometryDesc;
		asDesc.geometryInputCount = 1;
		m_topLevelAS = m_renderer->AllocateAccelerationStructure(asDesc);
	}
};