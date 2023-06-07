#pragma once
#include "IPipelineCache.h"
#include "ParaBlitDebug.h"

#include <unordered_map>

namespace PB
{
	class Renderer;
	class Device;

	struct PipelineDescHasher
	{
		u64 operator()(const GraphicsPipelineDesc& desc) const;
		u64 operator()(const ComputePipelineDesc& desc) const;
	};

	struct PipelineData
	{
		VkPipelineLayout m_layout = VK_NULL_HANDLE;
		VkPipeline m_pipeline = VK_NULL_HANDLE;
		bool m_isCompute = false;
		bool m_hasMeshShader = false;
	};

	class PipelineCache : public IPipelineCache
	{
	public:

		void Init(Renderer* renderer);

		void Destroy();

		PARABLIT_API Pipeline GetPipeline(const GraphicsPipelineDesc& desc) override;

		Pipeline GetPipeline(const ComputePipelineDesc& desc) override;

	private:

		inline void CreateCommonPipelineLayouts();

		PipelineData CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);

		PipelineData CreateComputePipeline(const ComputePipelineDesc& desc);

		Device* m_device = nullptr;
		VkDescriptorSetLayout m_masterSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_uboSetLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_commonGraphicsPipelineLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_commonGraphicsMeshShaderPipelineLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_commonComputePipelineLayout = VK_NULL_HANDLE;
		std::unordered_map<GraphicsPipelineDesc, PipelineData, PipelineDescHasher> m_graphicsPipelineCache;
		std::unordered_map<ComputePipelineDesc, PipelineData, PipelineDescHasher> m_computePipelineCache;
	};
};