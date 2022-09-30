#pragma once
#include "IBindingCache.h"
#include "CLib/Allocator.h"
#include "CLib/Vector.h"

namespace PB
{
	class BindingCache : public IBindingCache
	{
	public:

		BindingCache(CLib::Allocator* allocator);
		~BindingCache();

		PARABLIT_API void Append(const PB::UniformBufferView* uniformBuffers, u32 uniformBufferCount) override;
		PARABLIT_API void Append(const PB::ResourceView* resources, u32 resourceCount) override;
		PARABLIT_API void Append(const BindingLayout& bindings) override;
		PARABLIT_API void Append(const IBindingCache* bindings) override;

		PARABLIT_API void InsertFirst(const PB::UniformBufferView* uniformBuffers, u32 uniformBufferCount) override;
		PARABLIT_API void InsertFirst(const PB::ResourceView* resources, u32 resourceCount) override;
		PARABLIT_API void InsertFirst(const BindingLayout& bindings) override;
		PARABLIT_API void InsertFirst(const IBindingCache* bindings) override;

		PARABLIT_API void ClearUniformBindings() override;
		PARABLIT_API void ClearResourceBindings() override;
		PARABLIT_API void Clear() override;

		PARABLIT_API const BindingLayout& GetLayout() const override;
		PARABLIT_API operator const BindingLayout& () const override;

	private:

		UniformBufferView* m_uniformBufferViews = nullptr;
		u32 m_uniformBufferViewCount = 0;
		u32 m_uniformBufferArraySize = 0;
		ResourceView* m_resourceViews = nullptr;
		u32 m_resourceViewCount = 0;
		u32 m_resourceViewArraySize = 0;
		CLib::Allocator* m_allocator = nullptr;
		PB::BindingLayout m_layout;
	};
};