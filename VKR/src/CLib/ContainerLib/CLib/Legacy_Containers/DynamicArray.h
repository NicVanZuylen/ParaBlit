#pragma once

template<typename T, uint32_t Size = 0, bool AllowPlacement = false>
class DynamicArray
{
public:

	DynamicArray()
	{
		if constexpr (Size > 0)
		{
			m_size = Size;
			m_contents = AllocInternal(m_size);
		}
		else
		{
			m_size = 1;
			m_contents = AllocInternal(1);
		}
	}

	DynamicArray(uint32_t size, uint16_t expandRate = 1)
	{
		m_size = size;
		m_expandRate = expandRate;

		if (Size > 0 && m_size <= Size)
		{
			m_size = Size;
			m_contents = AllocInternal(m_size);
		}
		else
		{
			m_contents = AllocInternal(m_size);
		}
	}

	~DynamicArray()
	{
		m_count = 0;
		DeleteInternal();
	}

	inline void SetSize(const uint32_t& size)
	{
		if (m_size == size)
			return;

		assert(size >= m_count);

		byte* tmpContents = AllocInternal(size);

		uint32_t copySize = 0;
		if constexpr(AllowPlacement)
			copySize = m_size * sizeof(T);
		else
			copySize = m_count * sizeof(T);
		std::memcpy(tmpContents, m_contents, copySize);

		m_size = 0;
		DeleteInternal();
		m_contents = tmpContents;
		m_size = size;
	}

	T& operator [] (const uint32_t& index)
	{
		return GetContents()[index];
	}

	const T& operator [] (const uint32_t& index) const
	{
		return GetContents()[index];
	}

	T* begin()
	{
		return GetContents();
	}

	T* end()
	{
		if (m_count > 0)
			return &GetContents()[m_count - 1];
		else
			return GetContents();
	}

private:

	typedef unsigned char byte;

	inline byte* AllocInternal(uint32_t size)
	{
		if constexpr (AllowPlacement)
		{
			byte* ptr = nullptr;
			if (size > Size)
				ptr = reinterpret_cast<byte*>(malloc(sizeof(T) * static_cast<uint64_t>(size)));
			else
				ptr = m_fixedContents;
			uint32_t initializeRange = size - m_count;
			new (&(reinterpret_cast<T*>(ptr)[initializeRange])) T[initializeRange]();
			return ptr;
		}
		else
		{
			if (size > Size)
				return reinterpret_cast<byte*>(malloc(sizeof(T) * static_cast<uint64_t>(size)));
			else
				return m_fixedContents;
		}
	}

	inline void DeleteInternal()
	{
		if constexpr (AllowPlacement)
		{
			for (uint32_t i = 0; i < m_size; ++i)
			{
				GetContents()[i].~T();
			}
		}

		if (m_contents && m_contents != m_fixedContents)
		{
			free(m_contents);
			m_contents = nullptr;
		}
		else
			std::memset(m_fixedContents, 0, 2 + (Size * sizeof(T)));
	}

	inline T* GetContents()
	{
		return reinterpret_cast<T*>(m_contents);
	}

	byte* m_contents = nullptr;
	uint32_t m_size = 1;
	uint32_t m_count = 0;
	uint16_t m_expandRate = 1;
	byte m_fixedContents[2 + (Size * sizeof(T))];
};