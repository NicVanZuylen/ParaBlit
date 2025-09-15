#pragma once
#include <cassert>
#include <memory>
#include <initializer_list>

#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
#define CONTAINER_DEBUG_IMPLEMENTATION
#endif

#define CONTAINER_ASSERT_VALID assert(m_contents && "Dynamic Array Error: Attempting to perform operation with invalid contents!")

#pragma warning (push)
// IntelliSense isn't smart enough to release that m_fixedContents is a local array and thus always initialized. So we disable it. 6011 is a direct result of m_fixedContents being assigned to m_contents.
#pragma warning(disable : 26495)
#pragma warning(disable : 6011)

template <typename T, uint32_t StartSize = 0, bool AllowPlacement = false>
class DynamicArray
{
public:

	DynamicArray()
	{
		m_expandRate = 1;
		m_count = 0;
		
		if constexpr (StartSize > 0)
		{
			m_contents = reinterpret_cast<T*>(m_fixedContents); // Use fixed contents initially if thier size is above zero.
			m_size = StartSize;
		}
		else
		{
			m_contents = new T[1];
			memset(m_contents, 0, sizeof(T));
			m_size = 1;
		}
	}

	DynamicArray(uint32_t size, uint16_t expandRate = 1)
	{
		m_expandRate = 1;
		m_count = 0;

		if (size > StartSize)
		{
			m_contents = new T[size];
			m_size = size;
		}
		else
		{
			if constexpr (StartSize > 0)
			{
				m_contents = reinterpret_cast<T*>(m_fixedContents);
				m_size = StartSize;
			}
			else
			{
				m_contents = new T[size];
				m_size = size;
			}
		}
	}

	// Copy Constructor
	DynamicArray(const DynamicArray<T, StartSize>& other)
	{
		// Copy properties...
		m_count = other.Count();
		m_size = other.GetSize();
		m_expandRate = other.GetExpandRate();

		// Allocate contents equal to the other's count.
		if (other.Count() <= StartSize && StartSize > 0)
		{
			m_contents = reinterpret_cast<T*>(m_fixedContents);
			m_size = StartSize;
		}
		else
			m_contents = new T[m_size];

		// Copy contents.
		std::memcpy(m_contents, other.Data(), m_count * sizeof(T));
	}

	// Different Template Copy constructor.
	template<typename U, uint32_t OtherStartSize>
	DynamicArray(const DynamicArray<U, OtherStartSize>& other)
	{
		// Copy properties...
		m_count = other.Count();
		m_size = other.GetSize();
		m_expandRate = other.GetExpandRate();

		// Allocate contents equal to the other's count.
		if (other.Count() <= StartSize && StartSize > 0)
		{
			m_contents = reinterpret_cast<T*>(m_fixedContents);
			m_size = StartSize;
		}
		else
			m_contents = new T[m_size];

		// Copy contents.
		std::memcpy(m_contents, other.Data(), m_count * sizeof(T));
	}

	// Move constructor.
	DynamicArray(DynamicArray<T, StartSize>&& other)
	{
		m_count = other.m_count;
		m_size = other.m_size;
		m_expandRate = other.m_expandRate;

		// Copy pointer from other array and set the other array's pointer to null to release ownership.
		if (other.m_contents != reinterpret_cast<T*>(other.m_fixedContents))
		{
			m_contents = other.m_contents;
		}
		else if (m_count <= StartSize) // Other two cases still need a copy, as we can't steal the other array's local fixed contexts.
		{
			m_contents = reinterpret_cast<T*>(m_fixedContents);
			std::memcpy(m_contents, other.m_contents, m_count);
			m_size = StartSize;
		}

		other.m_contents = nullptr;
	}

	// C String assignment constructor.
	DynamicArray(const char* cString)
	{
		m_expandRate = 1;
		m_count = static_cast<uint32_t>(std::strlen(cString)) + 1;

		if (m_count <= StartSize)
		{
			m_contents = reinterpret_cast<T*>(m_fixedContents);
			std::memcpy(m_contents, cString, m_count - 1);
			m_contents[m_count - 1] = '\0';
			m_size = StartSize;
		}
		else
		{
			m_size = m_count;
			m_contents = new char[m_size];
			std::memcpy(m_contents, cString, m_count - 1);
			m_contents[m_count - 1] = '\0';
		}
	}

	DynamicArray(const std::initializer_list<T>& list)
	{
		m_count = static_cast<uint32_t>(list.size());
		m_size = m_count;
		if (m_size <= StartSize)
		{
			m_contents = reinterpret_cast<T*>(m_fixedContents);
			m_size = StartSize;
		}
		else
			m_contents = new T[m_size];
		m_expandRate = 1;

		// Copy initializer list contents into array.
		std::memcpy(m_contents, list.begin(), list.size() * sizeof(T));
	}

	inline ~DynamicArray()
	{
		Delete();
	}

	inline void Invalidate()
	{
		m_contents = nullptr;
	}

	// Getters

	inline uint32_t GetExpandRate() const
	{
		return m_expandRate;
	}

	inline uint32_t GetSize() const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
#endif

		return m_size;
	}

	inline uint32_t Count() const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
#endif

		return m_count;
	}

	// Setters

	inline void SetExpandRate(uint32_t nExpandRate)
	{
		m_expandRate = nExpandRate;
	}

	// Assigment to initializer list.
	void operator = (const std::initializer_list<T> list) 
	{
		Delete();

		m_contents = new T[list.size()];
		m_count = static_cast<uint32_t>(list.size());
		m_size = m_count;

		// Copy initializer list contents into array.
		std::memcpy(m_contents, list.begin(), list.size() * sizeof(T));
	}

	// Copy Assignment Operator.
	DynamicArray& operator = (const DynamicArray<T, StartSize>& other)
	{
		// Copy properties...
		m_count = other.Count();
		m_expandRate = other.GetExpandRate();

		// Allocate new array if this one is too small for the other's contents.
		if (m_size < m_count)
			SetSize(m_count);

		// Copy contents.
		std::memcpy(m_contents, other.Data(), m_count * sizeof(T));

		return *this;
	};

	// Different Template Copy assignment operator
	template<typename U, uint32_t OtherStartSize>
	DynamicArray& operator = (const DynamicArray<U, OtherStartSize>& other)
	{
		// Copy properties...
		m_count = other.Count();
		m_expandRate = other.GetExpandRate();

		// Allocate new array if this one is too small for the other's contents.
		if (m_size < m_count)
			SetSize(m_count);

		// Copy contents.
		std::memcpy(m_contents, other.Data(), m_count * sizeof(T));

		return *this;
	};

	DynamicArray<char, StartSize>& operator = (const char* cString)
	{
		m_expandRate = 1;
		m_count = static_cast<uint32_t>(std::strlen(cString)) + 1;

		Delete();

		if (m_count <= StartSize)
		{
			m_contents = m_fixedContents;
			std::memcpy(m_contents, cString, m_count - 1);
			m_contents[m_count - 1] = '\0';
			m_size = StartSize;
		}
		else
		{
			m_size = m_count;
			m_contents = new char[m_size];
			std::memcpy(m_contents, cString, m_count - 1);
			m_contents[m_count - 1] = '\0';
		}

		return *this;
	}

	// Move assignment operator.
	DynamicArray<T>& operator = (DynamicArray<T, StartSize>&& other) noexcept
	{
		m_count = other.m_count;
		m_size = m_count;
		m_expandRate = other.m_expandRate;

		Delete();

		// Copy pointer from other array and set the other array's pointer to null to release ownership.
		if (other.m_contents != reinterpret_cast<T*>(other.m_fixedContents))
		{
			m_contents = other.m_contents;
		}
		else if (m_count <= StartSize) // Other two cases still need a copy, as we can't steal the other array's local fixed contexts.
		{
			m_contents = reinterpret_cast<T*>(m_fixedContents);
			std::memcpy(m_contents, other.m_contents, m_count);
			m_size = StartSize;
		}

		other.m_contents = nullptr;

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
		CONTAINER_ASSERT_VALID;
#endif

		if (m_count < m_size)
		{
			m_contents[m_count++] = value; // Copy new value into existing space in the array.
		}
		else 
		{
			SetSize(m_size + m_expandRate); // Expand the array to make room for the new value.
			m_contents[m_count++] = value;
		}
	}

	/*
	Description: Constructs a new a value to the end of the array, and expands the array if there is no room for the new value. Requires a default constructor and move assignment operator.
	Speed: O(1), Possible Mem Alloc & Free
	*/
	template<typename... Args>
	inline T& Emplace(Args&&... args) 
	{
		if (m_count >= m_size)
			SetSize(m_size + m_expandRate);

		return (m_contents[m_count++] = std::move(T(args...)));
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
		CONTAINER_ASSERT_VALID;
		assert((nIndex >= 0 && nIndex <= m_count) && "Dynamic Array Error: Insertion index out of range.");
#endif

		// Make room for new value if there is none.
		if (m_count >= m_size)
			SetSize(m_size + m_expandRate);

		// Shift values further down the array.
		uint32_t nCopySize = (m_size - (nIndex + 1)) * sizeof(T);

		// Move contents.
		std::memcpy(&m_contents[nIndex + 1], &m_contents[nIndex], nCopySize);

		m_contents[nIndex] = value;
		++m_count;
	}

	/*
	Description: Insert another array into this one at the provided index.
	Speed: O(1), Likely Mem Alloc & Free
	Param:
		const DynArr<T>& arr: The array to insert onto this one.
		uint32_t nIndex: The index in this array to insert arr into.
	*/
	template<typename U, uint32_t OtherStartSize>
	inline void Insert(const DynamicArray<U, OtherStartSize>& arr, uint32_t nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
		assert((nIndex >= 0 && nIndex <= m_count) && "Dynamic Array Error: Insertion index out of range.");
#endif
		uint32_t nNewSize = m_count + arr.m_count;

		// Make room for new value if there is not enough.
		if (m_size < nNewSize)
			SetSize(nNewSize);

		// Shift values further down the array.
		uint32_t nCopySize = (m_size - (nIndex + arr.m_count)) * sizeof(T);

		// Move contents.
		std::memcpy(&m_contents[nIndex + arr.m_count], &m_contents[nIndex], nCopySize);

		// Copy other array into the gap.
		std::memcpy(&m_contents[nIndex], arr.m_contents, arr.m_count * sizeof(T));

		m_count = nNewSize;
	}

	/*
	Description: Extend contents with the values of another dynamic array.
	Speed: O(1), Possible Mem Alloc & Free
	Param:
	    const DynArr<T>& other: The array to append onto this one.
	*/
	template<typename U, uint32_t OtherStartSize>
	inline void Append(const DynamicArray<U, OtherStartSize>& other)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
		assert(other.Count() > 0 && "Dynamic Array Error: Attempted to append zero length array.");
#else
		if (other.Count() == 0)
			return;
#endif
		uint32_t newSize = m_count + other.Count();
		if(newSize > m_size)
			SetSize(newSize);

		// Copy contents from other array uint32_to this one.
		std::memcpy(&m_contents[m_count], other.Data(), other.Count() * sizeof(U));

		// Set new size and count.
		m_count = newSize;
	}

	// Append using += operator.
	template<typename U, uint32_t OtherStartSize>
	inline void operator += (const DynamicArray<U, OtherStartSize>& other)
	{
		Append(other);
	}

	/*
	Description: Extend contents with the values of a another C string.
	Speed: O(1), Possible Mem Alloc & Free
	Param:
		const char* cString:
	*/
	inline void Append(const char* cString)
	{
		uint32_t length = static_cast<uint32_t>(std::strlen(cString));
		uint32_t newSize = m_count + length;
		if (newSize > m_size)
			SetSize(newSize);

		memcpy(&m_contents[m_count - 1], cString, length);
		m_contents[newSize - 1] = '\0';
		m_count = newSize;
	}
	
	// Append C string using += operator.
	inline void operator += (const char* cString)
	{
		Append(cString);
	}

	/*
	Description: Set the size of the internal array.
	Param:
	    uint32_t nSize: The new size of the internal array.
	*/
	inline void SetSize(uint32_t nSize) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
		assert(nSize > 0 && "Dynamic Array Error: Attempted to resize array to zero.");
#endif
		// Nothing needs to be done if the sizes are equal.
		if (m_size == nSize)
			return;

		m_size = nSize;

		T* tmpContents;
		if (m_size > StartSize)
		{
			tmpContents = new T[m_size];
		}
		else
		{
			tmpContents = m_contents = reinterpret_cast<T*>(m_fixedContents);
			m_size = StartSize;
		}

		uint32_t nNewSize = nSize * sizeof(T);
		uint32_t nOldSize = m_count * sizeof(T);

		// Copy old contents to new array. If the new array is smaller, elements beyond its size will be lost.
		if(nNewSize < nOldSize)
		    std::memcpy(tmpContents, m_contents, nNewSize);
		else
			std::memcpy(tmpContents, m_contents, nOldSize);

		Delete();
		m_contents = tmpContents;

		if (m_count > m_size)
			m_count = m_size;
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
		CONTAINER_ASSERT_VALID;
		assert(nCount <= m_size && "Dynamic Array Error: Attempting to set count to a value higher than the internal array size.");
#endif
		m_count = nCount;
	}

	// Pop (remove)

	/*
	Description: Remove the final element from the array.
	Speed: O(1)
	*/
	inline void Pop()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
#endif
		if (m_count > 0)
			--m_count;
	}

	/*
	Description: Index through all objects in the array and remove the first element matching the input value (Slow).
	Speed: O(n)
	*/
	inline void Pop(const T value) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
#endif

		// Search through array for matching value and remove it if found.
		for (uint32_t i = 0; i < m_count; ++i) 
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
		CONTAINER_ASSERT_VALID;
		assert((nIndex >= 0 && nIndex < m_count) && "Dynamic Array Error: Subscript index out of range.");
#endif

		if(nIndex < m_size - 1) 
		{
			// Overlap contents of removed index with the contents after it.
			uint32_t nCopySize = (m_size - (nIndex + 1)) * sizeof(T);
			std::memcpy(&m_contents[nIndex], &m_contents[nIndex + 1], nCopySize);
		}

		// Decrease used slot count.
		--m_count;
	}

	/*
	Description: Trim off excess memory in the array.
	Speed: O(1), Mem Alloc & Free
	*/
	void ShrinkToFit()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
#endif

		if (m_contents == m_fixedContents)
			return;

		// Calculate amount of memory to free.
		if (m_count > StartSize)
			m_size = m_count;
		else
			m_size = StartSize;

		// Temporary pointer to the new array.
		T* tmpContents = m_fixedContents;
		if (m_size != StartSize)
			tmpContents = new T[m_size];

		// Copy old contents to new content array.
		std::memcpy(tmpContents, m_contents, m_size * sizeof(T));

		// m_contents is no longer useful, delete it.
		Delete();

		// Then set m_contents to the address of tmpContents
		m_contents = tmpContents;
	}

	/*
	Description: Retreive an element in the array via index.
	Return Type: T&
	Speed: O(1)
	*/
	T& operator [] (const uint32_t& nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		CONTAINER_ASSERT_VALID;
		assert((nIndex < m_count && nIndex >= 0) && "Dynamic Array Error: Subscript index out of range.");
#endif
		return m_contents[nIndex];
	}

	/*
	Description: Retreive an element in the array via index.
	Return Type: const T&
	Speed: O(1)
	*/
	const T& operator [] (const uint32_t& nIndex) const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		CONTAINER_ASSERT_VALID;
		assert((nIndex < m_count && nIndex >= 0) && "Dynamic Array Error: Subscript index out of range.");
#endif
		return m_contents[nIndex];
	}

	/*
	Description: Invalidate the contents of the array, allowing them to be overwritten.
	Speed: O(1)
	*/
	inline void Clear()
	{
		m_count = 0;
	}

	/*
	Description: Expose a pointer to the internal array.
	Return Type: T*
	Speed: O(1)
	*/
	inline T* Data() 
	{
		return m_contents;
	}

	/*
	Description: Expose a pointer to the internal array.
	Return Type: const T*
	Speed: O(1)
	*/
	inline const T* Data() const
	{
		return m_contents;
	}

	/*
	Description: Expose a pointer to the internal array as a C String.
	Return Type: const T*
	Speed: O(1)
	*/
	inline const char* CString() const
	{
		return reinterpret_cast<char*>(m_contents);
	}

	// Walking functions

	T* begin()
	{
		return m_contents;
	}

	T* end()
	{
		return &m_contents[m_count];
	}

	const T* begin() const
	{
		return m_contents;
	}

	const T* end() const
	{
		return &m_contents[m_count];
	}

	typedef bool(*CompFunctionPtr)(T lhs, T rhs);

	// Quick sort function. Takes in a function pointer used for sorting.
	void QuickSort(uint32_t nStart, uint32_t nEnd, CompFunctionPtr sortFunc)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		CONTAINER_ASSERT_VALID;
#endif

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
	inline uint32_t Partition(uint32_t nStart, uint32_t nEnd, CompFunctionPtr sortFunc)
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

	inline T* InternalAlloc(const uint32_t& size)
	{
		return new T[size];
	}

	inline void Delete()
	{
		if (m_contents && m_contents != reinterpret_cast<T*>(m_fixedContents))
			delete[] m_contents;
	}

	T* m_contents = reinterpret_cast<T*>(m_fixedContents);
	uint32_t m_size;
	uint32_t m_count;
	uint16_t m_expandRate;
	char m_fixedContents[2 + (StartSize * sizeof(T))]; // 2 Bytes for padding + additional bytes needed for the StartSize * Type.
};

using String = DynamicArray<char>;

template<uint32_t Size = 0>
using SizedString = DynamicArray<char, Size>;

#pragma warning(pop)