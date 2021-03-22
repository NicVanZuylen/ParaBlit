#pragma once
#include "IRenderer.h"

#include "CLib/Vector.h"

#include <cstdint>
#include <cassert>
#include <unordered_map>

template<typename ElementT, uint32_t ElementCount, uint32_t MaxElementsPerStagingBuffer>
class ManagedInstanceBuffer
{
public:

	ManagedInstanceBuffer() = default;

	ManagedInstanceBuffer(PB::IRenderer* renderer, PB::EBufferUsage usage = PB::EBufferUsage::VERTEX)
	{
		Init(renderer, usage);
	}

	~ManagedInstanceBuffer()
	{
		FlushChanges();
		m_renderer->FreeBuffer(m_buffer);
		m_renderer = nullptr;
		m_buffer = nullptr;
	}

	void Init(PB::IRenderer* renderer, PB::EBufferUsage usage = PB::EBufferUsage::VERTEX)
	{
		m_renderer = renderer;

		PB::BufferObjectDesc desc;
		desc.m_bufferSize = sizeof(ElementT) * ElementCount;
		desc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
		desc.m_usage = usage | PB::EBufferUsage::COPY_DST;
		m_buffer = m_renderer->AllocateBuffer(desc);

		m_curMappedStagingBuffer = nullptr;
	}

	PB::u8* MapElement(uint32_t elementLocation, uint32_t elementRegionStart, uint32_t elementRegionEnd)
	{
		MapStagingBuffer();
		uint32_t elementSrcLocation = elementRegionStart + (sizeof(ElementT) * m_mapRegions.Count());
		uint32_t elementDstLocation = (elementLocation * sizeof(ElementT)) + elementRegionStart;
		m_mapRegions.PushBack() = { elementSrcLocation + m_buffer->StagingBufferOffset(), elementDstLocation, elementRegionEnd - elementRegionStart };

		return m_curMappedStagingBuffer + elementSrcLocation;
		//return reinterpret_cast<PB::u8*>(&reinterpret_cast<ElementT*>(m_curMappedStagingBuffer)[elementLocation]) + elementRegionStart;
	}

	void FlushChanges()
	{
		if (m_curMappedStagingBuffer)
		{
			m_buffer->EndPopulate(m_mapRegions.Data(), m_mapRegions.Count());
			m_mapRegions.Clear();
			m_curMappedStagingBuffer = nullptr;
		}
	}

	PB::IBufferObject* GetBuffer()
	{
		return m_buffer;
	}

private:

	void MapStagingBuffer()
	{
		if (m_mapRegions.Count() == MaxElementsPerStagingBuffer)
			FlushChanges();

		constexpr uint32_t MapSize = (ElementCount < MaxElementsPerStagingBuffer ? ElementCount : MaxElementsPerStagingBuffer) * sizeof(ElementT);
		if (!m_curMappedStagingBuffer)
			m_curMappedStagingBuffer = m_buffer->BeginPopulate(MapSize);
	}

	PB::IRenderer* m_renderer = nullptr;
	PB::IBufferObject* m_buffer = nullptr;
	PB::u8* m_curMappedStagingBuffer = nullptr;
	CLib::Vector<PB::BufferCopyRegion, MaxElementsPerStagingBuffer, MaxElementsPerStagingBuffer> m_mapRegions{};
};