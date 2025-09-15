#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"

namespace PB
{
	class IBindingCache
	{
	public:

		PARABLIT_INTERFACE void Append(const PB::UniformBufferView* uniformBuffers, u32 uniformBufferCount) = 0;
		PARABLIT_INTERFACE void Append(const PB::ResourceView* resources, u32 resourceCount) = 0;
		PARABLIT_INTERFACE void Append(const BindingLayout& bindings) = 0;
		PARABLIT_INTERFACE void Append(const IBindingCache* bindings) = 0;

		PARABLIT_INTERFACE void InsertFirst(const PB::UniformBufferView* uniformBuffers, u32 uniformBufferCount) = 0;
		PARABLIT_INTERFACE void InsertFirst(const PB::ResourceView* resources, u32 resourceCount) = 0;
		PARABLIT_INTERFACE void InsertFirst(const BindingLayout& bindings) = 0;
		PARABLIT_INTERFACE void InsertFirst(const IBindingCache* bindings) = 0;

		PARABLIT_INTERFACE void ClearUniformBindings() = 0;
		PARABLIT_INTERFACE void ClearResourceBindings() = 0;
		PARABLIT_INTERFACE void Clear() = 0;

		PARABLIT_INTERFACE const BindingLayout& GetLayout() const = 0;
		PARABLIT_INTERFACE operator const BindingLayout& () const = 0;
	};
}