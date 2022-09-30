#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ICommandContext.h"
#include "CLib/Vector.h"
#include "CLib/FixedBlockAllocator.h"
#include "CLib/Allocator.h"

#include <unordered_map>



class ObjectDispatchList
{
private:
	struct DispatchSubList;
	struct ObjectDrawInstruction;

public:

	class DispatchObjectHandle
	{
	public:

		DispatchObjectHandle() = default;
		DispatchObjectHandle(DispatchSubList* list, ObjectDrawInstruction* instruction);

		~DispatchObjectHandle() = default;

	private:

		friend class ObjectDispatchList;

		DispatchSubList* m_dispatchList = nullptr;
		ObjectDrawInstruction* m_instruction = nullptr;
	};

	ObjectDispatchList() = default;
	~ObjectDispatchList();

	void Init(PB::IRenderer* renderer, CLib::Allocator* allocator, PB::Rect renderArea);

	DispatchObjectHandle AddObject(PB::Pipeline pipeline, const PB::IBufferObject* vertexBuffer, const PB::IBufferObject* indexBuffer, PB::BindingLayout bindingLayout, PB::DrawIndexedIndirectParams drawParams, const PB::IBufferObject* instanceBuffer);

	void RemoveDispatchObject(DispatchObjectHandle& handle);

	void SetIndirectParams(DispatchObjectHandle handle, PB::DrawIndexedIndirectParams params);

	void FlushCommandLists();

	void UpdateRenderArea(PB::Rect renderArea);

	void Update(PB::ICommandContext* commandContext);

	void Dispatch(PB::ICommandContext* commandContext, PB::RenderPass renderPass, PB::Framebuffer frameBuffer);

private:

	static constexpr const uint32_t IndirectParamsCount = 8;
	static constexpr const uint32_t IndirectParamsPageSize = IndirectParamsCount * sizeof(PB::DrawIndexedIndirectParams);

	struct ObjectDrawInstruction
	{
		const PB::IBufferObject* m_vertexBuffer = nullptr;
		const PB::IBufferObject* m_indexBuffer = nullptr;
		const PB::IBufferObject* m_instanceBuffer = nullptr;
		PB::u32 m_drawParamsBufferIndex = 0;
		PB::u32 m_drawParamsOffset = 0;
		PB::BindingLayout m_bindingLayout;

		ObjectDrawInstruction* m_prev = nullptr;
		ObjectDrawInstruction* m_next = nullptr;
	};

	struct BakedCommandList
	{
		PB::ICommandList* m_cmdList = nullptr;
		PB::Pipeline m_pipeline = 0;
	};

	struct DispatchSubList
	{
		ObjectDrawInstruction* InsertNewInstruction();
		void RemoveInstruction(ObjectDrawInstruction* instruction);
		void MarkForUpdate(PB::IRenderer* renderer);

		BakedCommandList* m_bakedCommandList = nullptr;
		CLib::FixedBlockAllocator m_drawInstructionStorage{ sizeof(ObjectDispatchList::ObjectDrawInstruction), sizeof(ObjectDispatchList::ObjectDrawInstruction) * 10 };
		ObjectDrawInstruction* m_instructionList = nullptr;
	};

	struct IndirectParamsPage
	{
		PB::IBufferObject* m_buffer = nullptr;
		PB::IBufferObject* m_stagingBuffer = nullptr;
		PB::u8* m_mappedPtr = nullptr;
	};

	struct IndirectParamsLocation
	{
		uint32_t m_buffer = 0;
		uint32_t m_offset = 0;
	};

	inline void WriteIndirectParams(IndirectParamsLocation location, const PB::DrawIndexedIndirectParams& params);

	inline void RecordDispatchSubListCommands(BakedCommandList& bakedList, PB::RenderPass renderPass, PB::Framebuffer frameBuffer);

	inline void AddIndirectParamsPage();

	PB::IRenderer* m_renderer = nullptr;
	CLib::Allocator* m_generalAllocator = nullptr;
	PB::Rect m_renderArea{ 0, 0, 0, 0 };
	CLib::Allocator m_bindingStorage{ sizeof(void*) * 256 };
	CLib::Vector<IndirectParamsPage> m_indirectParamsPages;
	CLib::Vector<IndirectParamsLocation, 8, 8> m_indirectParamsFreeList; // Offsets of free indirect draw parameter blocks in the indirect parameters buffer.
	CLib::Vector<IndirectParamsPage*> m_mappedParamsPages;
	std::unordered_map<PB::Pipeline, DispatchSubList*> m_dispatchSubLists;
	CLib::Vector<BakedCommandList, 16, 16> m_bakedCommandLists;
};