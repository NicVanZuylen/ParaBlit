#include "ResourceManager.h"

namespace PB
{
	ResourceManager::ResourceManager()
	{
		
	}

	ResourceManager::~ResourceManager()
	{

	}

	void ResourceManager::Init(Renderer* renderer)
	{
		m_renderer = renderer;

		CommandContextDesc contextDesc;
		contextDesc.m_renderer = reinterpret_cast<IRenderer*>(m_renderer);
		contextDesc.m_usage = PB_COMMAND_CONTEXT_USAGE_GRAPHICS;
		contextDesc.m_flags = PB_COMMAND_CONTEXT_PRIORITY;

		m_cmdContext.Init(contextDesc);
	}

	void ResourceManager::IssueCall(ResourceManagerCall& call)
	{
		if (m_cmdContext.GetState() == PB_COMMAND_CONTEXT_STATE_OPEN)
			m_cmdContext.Begin();

		call(m_cmdContext.GetCmdBuffer());
	}
};
