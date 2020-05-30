#pragma once
#include <cassert>
#include <memory>
#include <initializer_list>

#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
#define CONTAINER_DEBUG_IMPLEMENTATION
#endif

template <typename T, int Size, typename... Args>
class FixedArray
{
public:

	// Default expansion rate is 10.
	FixedArray()
	{
		m_nCount = 0;
	}

	// Copy constructor.
	template<typename U, uint32_t OtherSize>
	FixedArray(const FixedArray<U, OtherSize>& other)
	{
		// Copy properties...
		m_nCount = other.m_nCount;

		// Copy contents.
		if (Size <= OtherSize)
			std::memcpy(m_contents, other.Data(), Size * sizeof(T));
		else
			std::memcpy(m_contents, other.Data(), OtherSize * sizeof(T));
	}

	FixedArray(const std::initializer_list<T>& list)
	{
		uint32_t nListSize = static_cast<uint32_t>(list.size());
		if (nListSize > Size)
			nListSize = Size;

		// Copy initializer list contents into array.
		std::memcpy(m_contents, list.begin(), nListSize * sizeof(T));
		m_nCount = nListSize;
	}

	~FixedArray()
	{
	}

	// Getters

	uint32_t Count() const
	{
		return m_nCount;
	}

	// Setters

	// Assigment to initializer list.
	void operator = (const std::initializer_list<T> list)
	{
		uint32_t nListSize = static_cast<uint32_t>(list.size());
		if (nListSize > Size)
			nListSize = Size;

		// Copy initializer list contents into array.
		std::memcpy(m_contents, list.begin(), nListSize * sizeof(T));
		m_nCount = nListSize;
	}

	// Copy assignment operator
	template<typename U, uint32_t OtherSize>
	FixedArray<T, Size>& operator = (const FixedArray<U, OtherSize>& other)
	{
		// Copy properties...
		m_nCount = other.Count();

		// Copy contents.
		if(Size <= OtherSize)
			std::memcpy(m_contents, other.Data(), Size * sizeof(T));
		else
			std::memcpy(m_contents, other.Data(), OtherSize * sizeof(T));

		return *this;
	}

	// Push (add)

	/*
	Description: Appends a value to the end of the array, and expands the array if there is no room for the new value.
	Speed: O(1), Possible Mem Alloc & Free
	Param:
		const T& value: The new value to append.
	*/
	inline void Push(const T& value)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(m_nCount < Size && "Fixed Array Error: Attempted overflowing push.");
#endif

		m_contents[m_nCount++] = value;
	}

	/*
	Description: Constructs a new a value to the end of the array, and expands the array if there is no room for the new value. Requires a default constructor and move assignment operator.
	Speed: O(1), Possible Mem Alloc & Free
	*/
	inline T& Emplace(Args&&... args)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(m_nCount < Size && "Fixed Array Error: Attempted overflowing enplacement.");
#endif

		return (m_contents[m_nCount++] = std::move(T(args...)));
	}

	/*
	Description: Insert a new value to the provided index.
	Speed: O(1), Possible Mem Alloc & Free
	Param:
		const T& value: The value to insert into this array.
		uint32_t nIndex: The index to insert the value into.
	*/
	inline void Insert(const T& value, uint32_t nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert((nIndex >= 0 && nIndex <= m_nCount && m_nCount < Size) && "Fixed Array Error: Insertion index out of range.");
#endif

		// Shift values further down the array.
		uint32_t nCopySize = (Size - (nIndex + 1)) * sizeof(T);

		// Move contents.
		std::memcpy(&m_contents[nIndex + 1], &m_contents[nIndex], nCopySize);

		m_contents[nIndex] = value;
		++m_nCount;
	}

	/*
	Description: Insert another array into this one at the provided index.
	Speed: O(1)
	Param:
		const FixedArray<T, Size>& arr: The array to insert onto this one.
		uint32_t nIndex: The index in this array to insert arr into.
	*/
	template<typename U, uint32_t OtherSize>
	inline void Insert(const FixedArray<U, OtherSize> arr, uint32_t nIndex)
	{
		uint32_t nNewSize = m_nCount + arr.Count();

#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert((nIndex >= 0 && nIndex <= m_nCount && nNewSize <= Size) && "Dynamic Array Error: Insertion index out of range or not enough room for insertion.");
#endif
		// Shift values further down the array.
		uint32_t nCopySize = (Size - (nIndex + arr.Count())) * sizeof(T);

		// Move contents.
		std::memcpy(&m_contents[nIndex + arr.Count()], &m_contents[nIndex], nCopySize);

		// Copy other array into the gap.
		std::memcpy(&m_contents[nIndex], arr.Data(), arr.Count() * sizeof(T));

		m_nCount = nNewSize;
	}

	/*
	Description: Extend contents with the values of another fixed array.
	Speed: O(1)
	Param:
		const FixedArray<T, Size>& other: The array to append onto this one.
	*/
	template<typename U, uint32_t OtherSize>
	inline void Append(const FixedArray<U, OtherSize>& other)
	{
		uint32_t nNewLength = m_nCount + other.Count();

#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(other.Count() > 0 && "Fixed Array Error: Attempted to append zero length array.");
#else
		if (other.m_nCount == 0)
			return;
#endif

		uint32_t nOtherSize = other.Count() * sizeof(T);
		if (nNewLength > Size)
		{
			nNewLength = Size;
			nOtherSize = (Size - m_nCount) * sizeof(T);
		}

		// Copy contents from other array uint32_to this one.
		std::memcpy(&m_contents[m_nCount], other.Data(), nOtherSize);

		// Set new size and count.
		m_nCount = nNewLength;
	}

	// Append using += operator.
	template<typename U, uint32_t OtherSize>
	inline void operator += (const FixedArray<U, OtherSize>& other)
	{
		Append(other);
	}

	/*
	Description: Set the count of the dynamic array. (UNSAFE)
	Speed: O(1)
	Param:
		uint32_t nCount: The new amount of "valid" elements in this array.
	*/
	inline void SetCount(uint32_t nCount)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(nCount <= Size && "Fixed Array Error: Attempting to set count to a value higher than the internal array size.");
#endif

		m_nCount = nCount;
	}

	// Pop (remove)

	/*
	Description: Remove the final element from the array.
	Speed: O(1)
	*/
	inline void Pop()
	{
		if (m_nCount > 0)
			--m_nCount;
	}

	/*
	Description: Index through all objects in the array and remove the first element matching the input value (Slow).
	Speed: O(n)
	*/
	inline void Pop(const T value)
	{
		// Search through array for matching value and remove it if found.
		for (uint32_t i = 0; i < m_nCount; ++i)
		{
			if (std::memcmp(&m_contents[i], &value, sizeof(T)) == 0)
			{
				PopAt(i);
				return;
			}
		}
	}

	/*
	Description: Removes the value in the array at the specified index. The location of the removed value is replaced by its successor the junk value is moved to the end of the array.
	Speed: O(1)
	*/
	inline void PopAt(const uint32_t& nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert((nIndex >= 0 && nIndex < m_nCount) && "Fixed Array Error: Subscript index out of range.");
#endif

		if (nIndex < Size - 1)
		{
			// Overlap contents of removed index with the contents after it.
			uint32_t nCopySize = (Size - (nIndex + 1)) * sizeof(T);
			std::memcpy(&m_contents[nIndex], &m_contents[nIndex + 1], nCopySize);
		}

		// Decrease used slot count.
		--m_nCount;
	}

	/*
	Description: Retreive an element in the array via index.
	Return Type: T&
	Speed: O(1)
	*/
	T& operator [] (uint32_t nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		assert((nIndex < m_nCount&& nIndex >= 0) && "Fixed Array Error: Subscript index out of range.");
#endif
		return m_contents[nIndex];
	}

	/*
	Description: Retreive an element in the array via index.
	Return Type: const T&
	Speed: O(1)
	*/
	const T& operator [] (uint32_t nIndex) const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		assert((nIndex < m_nCount&& nIndex >= 0) && "Fixed Array Error: Subscript index out of range.");
#endif
		return m_contents[nIndex];
	}

	/*
	Description: Invalidate the contents of the array, allowing them to be overwritten.
	Speed: O(1)
	*/
	inline void Clear()
	{
		m_nCount = 0;
	}

	/*
	Description: Expose a pointer to the internal array.
	Return Type: T*
	Speed: O(1)
	*/
	T* Data()
	{
		return m_contents;
	}

	/*
	Description: Expose a pointer to the internal array.
	Return Type: const T*
	Speed: O(1)
	*/
	const T* Data() const
	{
		return m_contents;
	}

	T* begin()
	{
		return m_contents;
	}

	T* end()
	{
		return &m_contents[m_nCount];
	}

	const T* begin() const
	{
		return m_contents;
	}

	const T* end() const
	{
		return &m_contents[m_nCount];
	}

	typedef bool(*CompFunctionPtr)(T lhs, T rhs);

	// Quick sort function. Takes in a function pointer used for sorting.
	void QuickSort(uint32_t nStart, uint32_t nEnd, CompFunctionPtr sortFunc)
	{
		if (nStart < nEnd) // Is false when finished.
		{
			uint32_t nPartitionIndex = Partition(nStart, nEnd, sortFunc); // The splitting pouint32_t between sub-arrays/partitions.

			QuickSort(nStart, nPartitionIndex, sortFunc);
			QuickSort(nPartitionIndex + 1, nEnd, sortFunc);

			// Process repeats until the entire array is sorted.
		}
	}

private:

	// Partition function for quick sort algorithm.
	uint32_t Partition(uint32_t nStart, uint32_t nEnd, CompFunctionPtr sortFunc)
	{
		T nPivot = operator[](nEnd - 1);

		uint32_t smallPartition = nStart - 1; // AKA: i or the left partition slot.

		for (uint32_t j = smallPartition + 1; j < nEnd; ++j)
		{
			if (sortFunc(operator[](j), nPivot))
			{
				// Move selected left partition (i) slot.
				++smallPartition;

				// Move to left partition

				T tmp = operator[](smallPartition);

				operator[](smallPartition) = operator[](j);

				operator[](j) = tmp;
			}
		}
		// Swap next i and the pivot
		if (smallPartition < nEnd - 1)
		{
			T tmp = operator[](smallPartition + 1);

			operator[](smallPartition + 1) = operator[](nEnd - 1);

			operator[](nEnd - 1) = tmp;
		}

		return smallPartition;
	}

	T m_contents[Size];
	uint32_t m_nCount;
};