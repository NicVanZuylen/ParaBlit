#include "CmdContextPool.h"

namespace PB
{
	CmdContextPool::CmdContextPool()
	{
		m_contextPages[0] = new CommandContext[16];
		m_contextPages.SetCount(m_contextPages.GetSize());
	}

	CmdContextPool::~CmdContextPool()
	{
		for (auto& page : m_contextPages)
		{
			delete[] page;
		}
	}

	ICommandContext* CmdContextPool::Allocate()
	{
		if (m_gaps.Count() > 0)
		{
			ICommandContext* ptr = m_gaps[m_gaps.Count() - 1];
			m_gaps.Pop();
			return ptr;
		}

		CommandContext* ptr = &m_contextPages[m_contextPages.Count() - 1][m_contextCount++];

		// Allocate a new page if no more space is left.
		if (m_contextCount >= 16)
		{
			m_contextCount = 0;
			m_contextPages.Push(new CommandContext[16]);
		}

		return reinterpret_cast<ICommandContext*>(ptr);
	}

	void CmdContextPool::Free(ICommandContext* contextAddress)
	{
		m_gaps.Push(reinterpret_cast<CommandContext*>(contextAddress));
	}
}
