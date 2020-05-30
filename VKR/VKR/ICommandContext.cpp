#include "ICommandContext.h"
#include "CommandContext.h"
#include "Renderer.h"
#include "ParaBlitDebug.h"

namespace PB 
{
	ICommandContext::ICommandContext()
	{

	}

	ICommandContext::~ICommandContext()
	{

	}

	ICommandContext* CreateCommandContext(IRenderer* renderer)
	{
		PB_ASSERT(renderer);
		if (!renderer)
			return nullptr;

		return reinterpret_cast<Renderer*>(renderer)->GetContextPool().Allocate();
	}

	void DestroyCommandContext(ICommandContext*& cmdContext)
	{
		PB_ASSERT(cmdContext);
		if (!cmdContext)
			return;

		reinterpret_cast<CommandContext*>(cmdContext)->GetRenderer()->GetContextPool().Free(cmdContext);
		cmdContext = nullptr;
	}

	SCommandContext::SCommandContext(IRenderer* renderer)
	{
		m_ptr = CreateCommandContext(renderer);
	}

	SCommandContext::~SCommandContext()
	{
		DestroyCommandContext(m_ptr);
	}

	ICommandContext* SCommandContext::operator->()
	{
		return m_ptr;
	}
}
