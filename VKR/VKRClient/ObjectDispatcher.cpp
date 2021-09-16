#include "ObjectDispatcher.h"

ObjectDispatchList::~ObjectDispatchList()
{
	for (auto& commandList : m_bakedCommandLists)
	{
		if(commandList.m_cmdList)
			m_renderer->FreeCommandList(commandList.m_cmdList);
	}

	for (auto& drawParamPage : m_indirectParamsPages)
	{
		m_renderer->FreeBuffer(drawParamPage.m_stagingBuffer);
		m_renderer->FreeBuffer(drawParamPage.m_buffer);
	}

	for (auto& list : m_dispatchSubLists)
	{
		// Free all draw instructions.
		ObjectDrawInstruction* instruction = list.second->m_instructionList;
		while (instruction)
		{
			// Free binding storage.
			if (instruction->m_bindingLayout.m_uniformBuffers)
			{
				m_bindingStorage.Free(instruction->m_bindingLayout.m_uniformBuffers);
			}
			if (instruction->m_bindingLayout.m_resourceViews)
			{
				m_bindingStorage.Free(instruction->m_bindingLayout.m_resourceViews);
			}

			list.second->m_drawInstructionStorage.Free(instruction);
			instruction = instruction->m_next;
		}
		m_generalAllocator->Free(list.second);
	}
}

void ObjectDispatchList::Init(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::Rect renderArea)
{
	m_renderer = renderer;
	m_generalAllocator = allocator;
	m_renderArea = renderArea;
}

ObjectDispatchList::DispatchObjectHandle ObjectDispatchList::AddObject(PB::Pipeline pipeline, const PB::IBufferObject* vertexBuffer, const PB::IBufferObject* indexBuffer, PB::BindingLayout bindingLayout, PB::DrawIndexedIndirectParams drawParams, const PB::IBufferObject* instanceBuffer)
{
	auto it = m_dispatchSubLists.find(pipeline);
	if (it == m_dispatchSubLists.end())
	{
		m_dispatchSubLists[pipeline] = m_generalAllocator->Alloc<DispatchSubList>();
		it = m_dispatchSubLists.find(pipeline);

		auto* list = it->second;
		list->m_bakedCommandList = &m_bakedCommandLists.PushBackInit();
		list->m_bakedCommandList->m_pipeline = pipeline;
		list->m_instructionList = nullptr;
	}

	auto* list = it->second;
	auto* instruction = list->InsertNewInstruction();
	instruction->m_vertexBuffer = vertexBuffer;
	instruction->m_indexBuffer = indexBuffer;
	instruction->m_instanceBuffer = instanceBuffer;
	instruction->m_bindingLayout = bindingLayout;

	PB::UniformBufferView* uboStorage = reinterpret_cast<PB::UniformBufferView*>(m_bindingStorage.Alloc(sizeof(PB::UniformBufferView) * bindingLayout.m_uniformBufferCount));
	memcpy(uboStorage, bindingLayout.m_uniformBuffers, sizeof(PB::UniformBufferView) * bindingLayout.m_uniformBufferCount);
	instruction->m_bindingLayout.m_uniformBuffers = uboStorage;

	PB::ResourceView* viewStorage = reinterpret_cast<PB::ResourceView*>(m_bindingStorage.Alloc(sizeof(PB::ResourceView) * bindingLayout.m_resourceCount));
	memcpy(viewStorage, bindingLayout.m_resourceViews, sizeof(PB::ResourceView) * bindingLayout.m_resourceCount);
	instruction->m_bindingLayout.m_resourceViews = viewStorage;

	if (m_indirectParamsFreeList.Count() == 0)
		AddIndirectParamsPage();

	auto indirectParamsLocation = m_indirectParamsFreeList.PopBack();
	instruction->m_drawParamsBufferIndex = indirectParamsLocation.m_buffer;
	instruction->m_drawParamsOffset = indirectParamsLocation.m_offset;
	WriteIndirectParams(indirectParamsLocation, drawParams);

	list->MarkForUpdate(m_renderer);
	return DispatchObjectHandle(list, instruction);
}

void ObjectDispatchList::RemoveDispatchObject(DispatchObjectHandle& handle)
{
	ObjectDispatchList::DispatchSubList& dispatchList = *handle.m_dispatchList;
	dispatchList.RemoveInstruction(handle.m_instruction);
	dispatchList.MarkForUpdate(m_renderer);

	ObjectDispatchList::ObjectDrawInstruction& instruction = *handle.m_instruction;
	instruction.m_instanceBuffer = nullptr;

	ObjectDispatchList::IndirectParamsLocation& freeParamsLocation = m_indirectParamsFreeList.PushBack();
	freeParamsLocation.m_buffer = instruction.m_drawParamsBufferIndex;
	freeParamsLocation.m_offset = instruction.m_drawParamsOffset;

	m_bindingStorage.Free(instruction.m_bindingLayout.m_resourceViews);
	m_bindingStorage.Free(instruction.m_bindingLayout.m_uniformBuffers);
	instruction.m_bindingLayout.m_resourceViews = nullptr;
	instruction.m_bindingLayout.m_uniformBuffers = nullptr;

	handle.m_dispatchList = nullptr;
	handle.m_instruction = nullptr;
}

void ObjectDispatchList::SetIndirectParams(DispatchObjectHandle handle, PB::DrawIndexedIndirectParams params)
{
	IndirectParamsLocation location;
	location.m_buffer = handle.m_instruction->m_drawParamsBufferIndex;
	location.m_offset = handle.m_instruction->m_drawParamsOffset;

	WriteIndirectParams(location, params);
}

void ObjectDispatchList::FlushCommandLists()
{
	for (auto& subList : m_dispatchSubLists)
		subList.second->MarkForUpdate(m_renderer);
}

void ObjectDispatchList::UpdateRenderArea(PB::Rect renderArea)
{
	m_renderArea = renderArea;
	FlushCommandLists();
}

void ObjectDispatchList::Update(PB::ICommandContext* commandContext)
{
	for (IndirectParamsPage* page : m_mappedParamsPages)
	{
		page->m_stagingBuffer->Unmap();
		commandContext->CmdCopyBufferToBuffer(page->m_stagingBuffer, page->m_buffer, 0, 0, IndirectParamsPageSize);
		page->m_mappedPtr = nullptr;
	}
	m_mappedParamsPages.Clear();
}

void ObjectDispatchList::Dispatch(PB::ICommandContext* commandContext, PB::RenderPass renderPass, PB::Framebuffer frameBuffer)
{
	for (BakedCommandList& cmdList : m_bakedCommandLists)
	{
		if (cmdList.m_cmdList == nullptr)
			RecordDispatchSubListCommands(cmdList, renderPass, frameBuffer);

		commandContext->CmdExecuteList(cmdList.m_cmdList);
	}
}

void ObjectDispatchList::WriteIndirectParams(IndirectParamsLocation location, const PB::DrawIndexedIndirectParams& params)
{
	auto& page = m_indirectParamsPages[location.m_buffer];
	if (page.m_mappedPtr == nullptr)
	{
		page.m_mappedPtr = page.m_stagingBuffer->Map(0, IndirectParamsPageSize);
		m_mappedParamsPages.PushBack(&page);
	}
	page.m_stagingBuffer->PopulateWithDrawIndexedIndirectParams(&page.m_mappedPtr[location.m_offset], params);
}

void ObjectDispatchList::RecordDispatchSubListCommands(BakedCommandList& list, PB::RenderPass renderPass, PB::Framebuffer frameBuffer)
{
	const DispatchSubList& dispatchSubList = *m_dispatchSubLists[list.m_pipeline];

	PB::SCommandContext scopedContext(m_renderer);
	PB::CommandContextDesc contextDesc;
	contextDesc.m_renderer = m_renderer;
	contextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
	contextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;
	scopedContext->Init(contextDesc);
	scopedContext->Begin(renderPass, frameBuffer);

	// Bind this sub list's pipeline.
	scopedContext->CmdBindPipeline(list.m_pipeline);
	if (m_renderArea.w * m_renderArea.h > 0)
	{
		scopedContext->SetViewport(m_renderArea, 0.0f, 1.0f);
		scopedContext->SetScissor(m_renderArea);
	}

	ObjectDrawInstruction* ir = dispatchSubList.m_instructionList;
	while (ir)
	{
		// TODO: Some draw calls can probably share the same binding layouts. Perhaps draw instructions should be grouped Pipeline, and further down by binding layout (hash it?).
		// Preventing individual redundant bindings is probably pointless, as updating bindings in PB is efficient, so we don't need to track a binding state and check for redundancy.
		scopedContext->CmdBindResources(ir->m_bindingLayout);

		const PB::IBufferObject* vertexBuffers[2] = { ir->m_vertexBuffer, ir->m_instanceBuffer };
		if(vertexBuffers[0])
			scopedContext->CmdBindVertexBuffers(vertexBuffers, 2, ir->m_indexBuffer, PB::EIndexType::PB_INDEX_TYPE_UINT32);
		else if(ir->m_indexBuffer)
			scopedContext->CmdBindVertexBuffers(nullptr, 0, ir->m_indexBuffer, PB::EIndexType::PB_INDEX_TYPE_UINT32);
		scopedContext->CmdDrawIndexedIndirect(m_indirectParamsPages[ir->m_drawParamsBufferIndex].m_buffer, ir->m_drawParamsOffset);

		ir = ir->m_next;
	}

	scopedContext->End();
	list.m_cmdList = scopedContext->Return();
}

inline void ObjectDispatchList::AddIndirectParamsPage()
{
	PB::BufferObjectDesc indirectParamsBufferDesc{};
	indirectParamsBufferDesc.m_bufferSize = IndirectParamsPageSize;
	indirectParamsBufferDesc.m_usage = PB::EBufferUsage::INDIRECT_PARAMS | PB::EBufferUsage::COPY_DST;

	auto& page = m_indirectParamsPages.PushBack();
	page.m_buffer = m_renderer->AllocateBuffer(indirectParamsBufferDesc);

	indirectParamsBufferDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE;
	indirectParamsBufferDesc.m_usage = PB::EBufferUsage::COPY_SRC;
	page.m_stagingBuffer = m_renderer->AllocateBuffer(indirectParamsBufferDesc);

	page.m_mappedPtr = nullptr;

	// Add free locations from the new page.
	const PB::u32 indirectParamsSize = page.m_stagingBuffer->GetDrawIndexedIndirectParamsSize();
	const uint32_t bufferIndex = m_indirectParamsPages.Count() - 1;
	for (uint32_t i = 0; i < IndirectParamsCount; ++i)
	{
		auto& location = m_indirectParamsFreeList.PushBack();
		location.m_buffer = bufferIndex;
		location.m_offset = i * indirectParamsSize;
	}
}

ObjectDispatchList::DispatchObjectHandle::DispatchObjectHandle(DispatchSubList* list, ObjectDrawInstruction* instruction)
{
	m_dispatchList = list;
	m_instruction = instruction;
}

ObjectDispatchList::ObjectDrawInstruction* ObjectDispatchList::DispatchSubList::InsertNewInstruction()
{
	ObjectDrawInstruction* instruction = m_drawInstructionStorage.Alloc<ObjectDrawInstruction>();
	instruction->m_prev = nullptr;
	instruction->m_next = m_instructionList;

	if (m_instructionList)
		m_instructionList->m_prev = instruction;
	m_instructionList = instruction;
	return instruction;
}

void ObjectDispatchList::DispatchSubList::RemoveInstruction(ObjectDrawInstruction* instruction)
{
	if(instruction->m_prev)
		instruction->m_prev->m_next = instruction->m_next;
	if(instruction->m_next)
		instruction->m_next->m_prev = instruction->m_prev;

	m_drawInstructionStorage.Free(instruction);
}

void ObjectDispatchList::DispatchSubList::MarkForUpdate(PB::IRenderer* renderer)
{
	if (m_bakedCommandList->m_cmdList)
	{
		renderer->FreeCommandList(m_bakedCommandList->m_cmdList);
		m_bakedCommandList->m_cmdList = nullptr;
	}
}
