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
		contextDesc.m_usage = ECommandContextUsage::GRAPHICS;
		contextDesc.m_flags = ECommandContextFlags::PRIORITY;

		m_cmdContext.Init(contextDesc);
	}

	void ResourceManager::IssueCall(ResourceManagerCall& call)
	{
		if (m_cmdContext.GetState() == ECmdContextState::OPEN)
			m_cmdContext.Begin();

		call(m_cmdContext.GetCmdBuffer());
	}
};
