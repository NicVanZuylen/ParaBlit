#include "CmdContextPool.h"

namespace PB
{
	CmdContextPool::CmdContextPool()
	{
		m_contextPages[0] = new CommandContext[16];
		m_contextPages.SetCount(m_contextPages.Capacity());
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
		std::lock_guard<std::mutex> m_lock(m_allocatorLock);
		if (m_gaps.Count() > 0)
		{
			ICommandContext* ptr = m_gaps.Back();
			m_gaps.PopBack();
			return ptr;
		}

		CommandContext* ptr = &m_contextPages.Back()[m_contextCount++];

		// Allocate a new page if no more space is left.
		if (m_contextCount >= 16)
		{
			m_contextCount = 0;
			m_contextPages.PushBack(new CommandContext[16]);
		}

		return reinterpret_cast<ICommandContext*>(ptr);
	}

	void CmdContextPool::Free(ICommandContext* contextAddress)
	{
		std::lock_guard<std::mutex> m_lock(m_allocatorLock);
		m_gaps.PushBack(reinterpret_cast<CommandContext*>(contextAddress));
	}
}
