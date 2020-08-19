#pragma once
#include "IPipelineCache.h"

#include "vulkan/vulkan.h"

#include <unordered_map>

namespace PB
{
	class Renderer;
	class Device;

	struct PipelineDescHasher
	{
		u64 operator()(const PipelineDesc& desc) const;
	};

	struct PipelineData
	{
		VkPipelineLayout m_layout = VK_NULL_HANDLE;
		VkPipeline m_pipeline = VK_NULL_HANDLE;
	};

	class PipelineCache : public IPipelineCache
	{
	public:

		void Init(Renderer* renderer);

		void Destroy();

		PARABLIT_API Pipeline GetPipeline(const PipelineDesc& desc) override;

	private:

		PipelineData CreatePipeline(const PipelineDesc& desc);

		Device* m_device = nullptr;
		VkDescriptorSetLayout m_driSetLayout = VK_NULL_HANDLE;
		std::unordered_map<PipelineDesc, PipelineData, PipelineDescHasher> m_pipelineCache;
	};
};