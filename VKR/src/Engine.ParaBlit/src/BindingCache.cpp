#include "BindingCache.h"
#include "ParaBlitDebug.h"

namespace PB
{
	BindingCache::BindingCache(CLib::Allocator* allocator)
	{
		m_allocator = allocator;
	}

	BindingCache::~BindingCache()
	{
		if (m_uniformBufferViews)
			m_allocator->Free(m_uniformBufferViews);

		if (m_resourceViews)
			m_allocator->Free(m_resourceViews);
	}

	void BindingCache::Append(const PB::UniformBufferView* uniformBuffers, u32 uniformBufferCount)
	{
		PB_ASSERT(uniformBufferCount > 0 && uniformBuffers != nullptr);
		u32 oldCount = m_uniformBufferViewCount;

		m_uniformBufferViewCount += uniformBufferCount;
		if (m_uniformBufferArraySize < m_uniformBufferViewCount)
		{
			UniformBufferView* oldViews = m_uniformBufferViews;
			m_uniformBufferViews = reinterpret_cast<UniformBufferView*>(m_allocator->Alloc(sizeof(UniformBufferView) * m_uniformBufferViewCount));
			m_uniformBufferArraySize = m_uniformBufferViewCount;

			if (oldViews)
			{
				memcpy(m_uniformBufferViews, oldViews, sizeof(UniformBufferView) * oldCount);
				m_allocator->Free(oldViews);
			}
		}

		memcpy(&m_uniformBufferViews[oldCount], uniformBuffers, sizeof(UniformBufferView) * uniformBufferCount);

		m_layout.m_uniformBufferCount = m_uniformBufferViewCount;
		m_layout.m_uniformBuffers = m_uniformBufferViews;
	}

	void BindingCache::Append(const PB::ResourceView* resources, u32 resourceCount)
	{
		PB_ASSERT(resourceCount > 0 && resources != nullptr);
		u32 oldCount = m_resourceViewCount;

		m_resourceViewCount += resourceCount;
		if (m_resourceViewArraySize < m_resourceViewCount)
		{
			ResourceView* oldViews = m_resourceViews;
			m_resourceViews = reinterpret_cast<ResourceView*>(m_allocator->Alloc(sizeof(ResourceView) * m_resourceViewCount));
			m_resourceViewArraySize = m_resourceViewCount;

			if (oldViews)
			{
				memcpy(m_resourceViews, oldViews, sizeof(ResourceView) * oldCount);
				m_allocator->Free(oldViews);
			}
		}

		memcpy(&m_resourceViews[oldCount], resources, sizeof(ResourceView) * resourceCount);

		m_layout.m_resourceCount = m_resourceViewCount;
		m_layout.m_resourceViews = m_resourceViews;
	}

	void BindingCache::Append(const BindingLayout& bindings)
	{
		if (bindings.m_uniformBufferCount > 0)
			Append(bindings.m_uniformBuffers, bindings.m_uniformBufferCount);

		if (bindings.m_resourceCount > 0)
			Append(bindings.m_resourceViews, bindings.m_resourceCount);
	}

	void BindingCache::Append(const IBindingCache* bindings)
	{
		Append(bindings->GetLayout());
	}

	void BindingCache::InsertFirst(const PB::UniformBufferView* uniformBuffers, u32 uniformBufferCount)
	{
		PB_ASSERT(uniformBufferCount > 0 && uniformBuffers != nullptr);
		u32 oldCount = m_uniformBufferViewCount;

		m_uniformBufferViewCount += uniformBufferCount;
		if (m_uniformBufferArraySize < m_uniformBufferViewCount)
		{
			UniformBufferView* oldViews = m_uniformBufferViews;
			m_uniformBufferViews = reinterpret_cast<UniformBufferView*>(m_allocator->Alloc(sizeof(UniformBufferView) * m_uniformBufferViewCount));
			m_uniformBufferArraySize = m_uniformBufferViewCount;

			memcpy(m_uniformBufferViews, uniformBuffers, sizeof(UniformBufferView) * uniformBufferCount);

			if (oldViews)
			{
				memcpy(&m_uniformBufferViews[uniformBufferCount], oldViews, sizeof(UniformBufferView) * oldCount);
				m_allocator->Free(oldViews);
			}
		}
		else 
		{
			memcpy(&m_uniformBufferViews[uniformBufferCount], m_uniformBufferViews, sizeof(UniformBufferView) * oldCount);
			memcpy(m_uniformBufferViews, uniformBuffers, sizeof(UniformBufferView) * uniformBufferCount);
		}

		m_layout.m_uniformBufferCount = m_uniformBufferViewCount;
		m_layout.m_uniformBuffers = m_uniformBufferViews;
	}

	void BindingCache::InsertFirst(const PB::ResourceView* resources, u32 resourceCount)
	{
		PB_ASSERT(resourceCount > 0 && resources != nullptr);
		u32 oldCount = m_resourceViewCount;

		m_resourceViewCount += resourceCount;
		if (m_resourceViewArraySize < m_resourceViewCount)
		{
			ResourceView* oldViews = m_resourceViews;
			m_resourceViews = reinterpret_cast<ResourceView*>(m_allocator->Alloc(sizeof(ResourceView) * m_resourceViewCount));
			m_resourceViewArraySize = m_resourceViewCount;
			memcpy(m_resourceViews, oldViews, sizeof(ResourceView) * oldCount);

			memcpy(m_resourceViews, resources, sizeof(ResourceView) * resourceCount);

			if (oldViews)
			{
				memcpy(&m_resourceViews[resourceCount], oldViews, sizeof(ResourceView) * oldCount);
				m_allocator->Free(oldViews);
			}
		}
		else
		{
			memcpy(&m_resourceViews[resourceCount], m_resourceViews, sizeof(ResourceView) * oldCount);
			memcpy(m_resourceViews, resources, sizeof(ResourceView) * resourceCount);
		}

		m_layout.m_resourceCount = m_resourceViewCount;
		m_layout.m_resourceViews = m_resourceViews;
	}

	void BindingCache::InsertFirst(const BindingLayout& bindings)
	{
		if (bindings.m_uniformBufferCount > 0)
			InsertFirst(bindings.m_uniformBuffers, bindings.m_uniformBufferCount);

		if (bindings.m_resourceCount > 0)
			InsertFirst(bindings.m_resourceViews, bindings.m_resourceCount);
	}

	void BindingCache::InsertFirst(const IBindingCache* bindings)
	{
		InsertFirst(bindings->GetLayout());
	}

	void BindingCache::ClearUniformBindings()
	{
		m_uniformBufferViewCount = 0;
		m_layout.m_uniformBufferCount = 0;
	}

	void BindingCache::ClearResourceBindings()
	{
		m_resourceViewCount = 0;
		m_layout.m_resourceCount = 0;
	}

	void BindingCache::Clear()
	{
		ClearUniformBindings();
		ClearResourceBindings();
	}

	const BindingLayout& BindingCache::GetLayout() const
	{
		return m_layout;
	}

	BindingCache::operator const BindingLayout& () const
	{
		return GetLayout();
	}
};
