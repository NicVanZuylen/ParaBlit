#pragma once
#include "IPipelineCache.h"
#include "ParaBlitDebug.h"

#include <unordered_map>

namespace PB
{
	class Renderer;
	class Device;
	class BufferObject;

	struct PipelineDescHasher
	{
		u64 operator()(const GraphicsPipelineDesc& desc) const;
		u64 operator()(const ComputePipelineDesc& desc) const;
		u64 operator()(const RayTracingPipelineDesc& desc) const;
	};

	struct RayTracingSBTData
	{
		BufferObject* m_sbtBuffer = nullptr;
		VkStridedDeviceAddressRegionKHR m_rgenRegion{}; // Raygen handle region.
		VkStridedDeviceAddressRegionKHR m_missRegion{}; // Miss handle region.
		VkStridedDeviceAddressRegionKHR m_hitRegion{}; // Closest hit handle region.
		VkStridedDeviceAddressRegionKHR m_callRegion{}; // Callable handle region.
	};

	struct PipelineData
	{
		VkPipelineLayout m_layout = VK_NULL_HANDLE;
		VkPipeline m_pipeline = VK_NULL_HANDLE;
		RayTracingSBTData m_raytracingData{};
		bool m_isCompute = false;
		bool m_hasMeshShader = false;
		bool m_isRaytracing = false;
	};

	class PipelineCache : public IPipelineCache
	{
	public:

		static void GetExtensionFunctions(VkInstance instance);

		void Init(Renderer* renderer);

		void Destroy();

		Pipeline GetPipeline(const GraphicsPipelineDesc& desc) override;

		Pipeline GetPipeline(const ComputePipelineDesc& desc) override;

		Pipeline GetPipeline(const RayTracingPipelineDesc& desc) override;

	private:

		static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHRFunc;
		static PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHRFunc;

		inline void CreateCommonPipelineLayouts();

		PipelineData CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);

		PipelineData CreateComputePipeline(const ComputePipelineDesc& desc);

		PipelineData CreateRayTracingPipeline(const RayTracingPipelineDesc& desc);

		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		VkDescriptorSetLayout m_masterSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_uboSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_asSetLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_commonGraphicsPipelineLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_commonGraphicsMeshShaderPipelineLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_commonComputePipelineLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_commonRaytracingPipelineLayout = VK_NULL_HANDLE;
		std::unordered_map<GraphicsPipelineDesc, PipelineData, PipelineDescHasher> m_graphicsPipelineCache;
		std::unordered_map<ComputePipelineDesc, PipelineData, PipelineDescHasher> m_computePipelineCache;
		std::unordered_map<RayTracingPipelineDesc, PipelineData, PipelineDescHasher> m_raytracingPipelineCache;
	};
};