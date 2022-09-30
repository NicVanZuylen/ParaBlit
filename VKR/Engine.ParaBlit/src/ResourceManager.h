#pragma once
#include "ParaBlitApi.h"
#include "CommandContext.h"

#include <functional>

namespace PB 
{
	class Renderer;

	class ResourceManager
	{
	public:

		using ResourceManagerCall = std::function<void(VkCommandBuffer)>;

		PARABLIT_API ResourceManager();

		PARABLIT_API ~ResourceManager();

		PARABLIT_API void Init(Renderer* renderer);

		PARABLIT_API inline void IssueCall(ResourceManagerCall& call);

	private:

		Renderer* m_renderer = nullptr;
		CommandContext m_cmdContext;
	};
}

