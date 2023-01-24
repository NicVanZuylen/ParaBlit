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

		return reinterpret_cast<Renderer*>(renderer)->AllocateCommandContext();
	}

	void DestroyCommandContext(ICommandContext*& cmdContext)
	{
		PB_ASSERT(cmdContext);
		if (!cmdContext)
			return;

		CommandContext* internalContext = reinterpret_cast<CommandContext*>(cmdContext);
		internalContext->GetRenderer()->FreeCommandContext(internalContext);
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

	bool SubresourceRange::operator==(const SubresourceRange& other) const
	{
		return m_arrayCount == other.m_arrayCount && m_firstArrayElement == other.m_firstArrayElement && m_mipCount == other.m_mipCount && m_baseMip == other.m_baseMip;
	}
}
