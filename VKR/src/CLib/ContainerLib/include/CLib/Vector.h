#pragma once
#include <cstdlib>
#include <cstring>
#include <functional>
#include <type_traits>

namespace CLib
{
	inline std::function<void* (unsigned long long)> vectorAllocFunc = malloc;
	inline std::function<void(void*)> vectorFreeFunc = free;

#define VECTOR_SELF_T Vector<T, FixedCapacity, ExpandRate, UseTypeSafety>
#define VECTOR_OTHER_T Vector<T, OFixedCapacity, OExpandRate, OUseTypeSafety>
#define VECTOR_OTHER_TEMPLATE template<unsigned int OFixedCapacity, unsigned int OExpandRate, bool OUseTypeSafety>

	template <typename T>
	concept VectorTCanCopyAssign = std::is_copy_assignable<T>::value || std::is_trivially_copyable<T>::value;

	template <typename T>
	concept VectorTSafeCompatible = std::is_default_constructible<T>::value && std::is_copy_constructible<T>::value;

	template <typename T, bool UseTypeSafety>
	concept VectorTypeSafe = (UseTypeSafety == false && VectorTCanCopyAssign<T>) || (UseTypeSafety == true && VectorTSafeCompatible<T>);

	// Standard Custom Vector class by Nicholas Van Zuylen. Enable 'UseTypeSafety' to safely contain objects that are not trivially copyable.
	template<typename T, unsigned int FixedCapacity = 0, unsigned int ExpandRate = 1, bool UseTypeSafety = false>
	requires VectorTypeSafe<T, UseTypeSafety>
	class Vector
	{
	public:

		// ------------------------------ Construction ------------------------------

		Vector() noexcept
		{
			m_capacity = FixedCapacity;
		}

		Vector(const unsigned int& startCapacity) noexcept
		{
			if (startCapacity > FixedCapacity)
			{
				m_contents = Alloc(startCapacity);
				m_capacity = startCapacity;
			}
			else
				m_capacity = FixedCapacity;
		}

		// Copy constructor
		Vector(const VECTOR_SELF_T& other) noexcept
		{
			CopyFrom(other);
		}

		// Move constructor
		Vector(VECTOR_SELF_T&& other) noexcept
		{
			MoveFrom(other);
		}

		// Initializer list construction
		Vector(const std::initializer_list<T>& list)
		{
			CopyFromInitList(list);
		}

		~Vector()
		{
			DestructAllElements();

			Free(m_contents);
			m_count = 0;
			m_capacity = 0;
			m_contents = nullptr;
		}

		// ------------------------------- Read Access -------------------------------

		inline T* begin() { return m_contents; }
		inline const T* begin() const { return m_contents; }
		inline T* end() { return &m_contents[m_count]; }
		inline const T* end() const { return &m_contents[m_count]; }
		inline T* Data() { return m_contents; }
		inline const T* Data() const { return m_contents; }
		inline T& Front() { return m_contents[0]; }
		inline const T& Front() const { return m_contents[0]; }
		inline T& Back() { return m_contents[m_count - 1]; }
		inline const T& Back() const { return m_contents[m_count - 1]; }
		inline T& operator [] (const unsigned int& i) { return m_contents[i]; }
		inline const T& operator [](const unsigned int& i) const { return m_contents[i]; }
		inline T& At(const unsigned int& i) { return m_contents[i]; }
		inline const T& At(const unsigned int& i) const { return m_contents[i]; }
		inline const unsigned int& Count() const { return m_count; }
		inline const unsigned int& Capacity() const { return m_capacity; }

		// ------------------------------ Write Access -------------------------------

		inline T& PushBack(const T& val)
		{
			if (m_count + 1 > m_capacity)
				SetCapacity(m_count + ExpandRate);

			if constexpr (UseTypeSafety == false)
			{
				return m_contents[m_count++] = val;
			}
			else
			{
				new (&m_contents[m_count]) T();
				return m_contents[m_count++] = val;
			}
		}

		inline T& PushBack()
		{
			if (m_count + 1 > m_capacity)
				SetCapacity(m_count + ExpandRate);

			if constexpr (UseTypeSafety)
			{
				new (&m_contents[m_count]) T();
			}

			return m_contents[m_count++];
		}

		inline T& PopBack()
		{
			if (m_count > 0)
			{
				if constexpr (UseTypeSafety)
				{
					m_contents[m_count - 1].~T();
				}

				return m_contents[--m_count];
			}

			return m_contents[0];
		}

		// Requires T to have a move constructor.
		template<typename... Args>
		inline T& PushBackInit(Args&&... args)
		{
			if (m_count + 1 > m_capacity)
				SetCapacity(m_count + ExpandRate);

			if constexpr (UseTypeSafety)
			{
				new (&m_contents[m_count]) T(args...);

				return m_contents[m_count++];
			}
			else
			{
				return m_contents[m_count++] = std::move(T(args...));
			}
		}

		VECTOR_OTHER_TEMPLATE
		inline void Append(const VECTOR_OTHER_T& other)
		{
			unsigned int newCount = m_count + other.Count();
			if (newCount > m_capacity)
			{
				m_capacity = newCount;
				SetCapacity(m_capacity);
			}

			memcpy(&m_contents[m_count], other.Data(), sizeof(T) * other.Count());
			m_count = newCount;
		}

		inline void Reserve(const unsigned int& newCapacity) { SetCapacity(newCapacity); }
		inline void SetCount(const unsigned int& newCount)
		{
			if constexpr (UseTypeSafety)
			{
				if (newCount < m_count)
				{
					for (unsigned int i = newCount; i < m_count; ++i)
					{
						m_contents[i].~T();
					}
				}
			}

			if (newCount > m_capacity)
				SetCapacity(newCount);

			if constexpr (UseTypeSafety)
			{
				if (newCount > m_count)
				{
					for (unsigned int i = m_count; i < newCount; ++i)
					{
						new (&m_contents[i]) T();
					}
				}
			}

			m_count = newCount;
		}

		inline void Clear() 
		{
			DestructAllElements();

			m_count = 0;
		}

		inline void Trim() { SetCapacity(FixedCapacity); }

		// ---------------------------- Write Operators -------------------------------

		// Copy assignment
		inline VECTOR_SELF_T& operator = (const VECTOR_SELF_T& other)
		{
			if constexpr (UseTypeSafety)
			{
				Clear();
			}

			CopyFrom(other);
			return *this;
		}

		// Move assignment
		inline VECTOR_SELF_T& operator = (VECTOR_SELF_T&& other)
		{
			DestructAllElements();
			MoveFrom(other);
			return *this;
		}

		// Initializer list assignment
		inline VECTOR_SELF_T& operator = (const std::initializer_list<T>& list)
		{
			if constexpr (UseTypeSafety)
			{
				Clear();
			}

			CopyFromInitList(list);
			return *this;
		}

		// Append operator
		VECTOR_OTHER_TEMPLATE
		inline VECTOR_SELF_T& operator += (const VECTOR_OTHER_T& other)
		{
			Append(other);
			return *this;
		}

	private:

		VECTOR_OTHER_TEMPLATE
		inline void CopyFrom(const VECTOR_OTHER_T& other)
		{
			m_count = other.Count();
			if (m_capacity < m_count)
			{
				m_capacity = m_count;
				m_contents = Alloc(m_capacity);
			}

			if constexpr (UseTypeSafety) 
			{
				for (unsigned int i = 0; i < m_count; ++i)
				{
					new (&m_contents[i]) T(other.m_contents[i]);
				}
			}
			else
			{

				memcpy(m_contents, other.Data(), sizeof(T) * m_count);
			}
		}

		inline void MoveFrom(Vector<T, FixedCapacity, ExpandRate>& other)
		{
			if (m_contents != reinterpret_cast<T*>(m_fixedContents))
			{
				Free(m_contents);
			}

			m_count = other.m_count;

			// Fixed contents cannot be moved, so copy the other vector if it is using them.
			if (other.m_contents != reinterpret_cast<T*>(other.m_fixedContents))
			{
				m_contents = other.m_contents;
				m_capacity = other.m_capacity;
			}
			else if (other.m_count <= FixedCapacity)
			{
				if constexpr (UseTypeSafety)
				{
					Clear();
				}
				
				m_contents = reinterpret_cast<T*>(m_fixedContents);
				m_capacity = FixedCapacity;

				if constexpr (UseTypeSafety)
				{
					CopyFrom(other);
				}
				else
				{
					memcpy(m_contents, other.m_contents, sizeof(T) * m_count);
				}
			}
			else
			{
				Clear();
				CopyFrom(other);
			}

			other.m_capacity = 0;
			other.m_count = 0;
			other.m_contents = nullptr;
		}

		inline void CopyFromInitList(const std::initializer_list<T>& list)
		{
			// Copy list contents.
			if constexpr (UseTypeSafety)
			{
				m_count = list.size();
				if (m_capacity < m_count)
				{
					m_capacity = m_count;
					m_contents = Alloc(m_capacity);
				}

				for (unsigned int i = 0; i < m_count; ++i)
				{
					new (&m_contents[i]) T(list[i]);
				}
			}
			else
			{
				SetCount(list.size());
				memcpy(m_contents, list.begin(), sizeof(T) * m_count);
			}
		}

		inline T* Alloc(const unsigned int& capacity)
		{
			return reinterpret_cast<T*>(vectorAllocFunc(sizeof(T) * capacity));
		}

		inline void Free(T* mem)
		{
			if (mem != reinterpret_cast<T*>(m_fixedContents) && mem)
			{
				vectorFreeFunc(mem);
			}
		}

		inline void SetCapacity(const unsigned int& newCapacity)
		{
			// Nothing to be done if the new capacity is within the capacity of the fixed contents, and we're already using the fixed contents.
			if (newCapacity <= FixedCapacity && m_contents == reinterpret_cast<T*>(m_fixedContents))
				return;

			// Use fixed contents if the new capacity fits within the fixed capacity.
			T* newContents = reinterpret_cast<T*>(m_fixedContents);

			if constexpr (UseTypeSafety) // If type safe, destruct trimmed elements.
			{
				if (newCapacity < m_count)
				{
					for (unsigned int i = newCapacity; i < m_count; ++i)
					{
						m_contents[i].~T();
					}
				}
			}

			if (newCapacity <= FixedCapacity)
			{
				m_capacity = FixedCapacity;
			}
			else
			{
				m_capacity = newCapacity;
				newContents = Alloc(m_capacity);
			}

			if (m_count > m_capacity) // Trim old contents.
				m_count = m_capacity;

			if constexpr (UseTypeSafety) // If type safe, copy construct new contents and destruct src elements.
			{
				if constexpr (std::is_move_constructible<T>::value == true) // Prefer move construction if available. It's much faster.
				{
					for (unsigned int i = 0; i < m_count; ++i)
					{
						new (&newContents[i]) T(std::move(m_contents[i]));
					}
				}
				else // Copy construct.
				{
					for (unsigned int i = 0; i < m_count; ++i)
					{
						new (&newContents[i]) T(m_contents[i]);
					}

					DestructAllElements();
				}
			}
			else if constexpr (std::is_trivially_copyable<T>::value == true)
			{
				memcpy(newContents, m_contents, sizeof(T) * m_count);
			}
			else
			{
				static_assert(std::is_copy_assignable<T>::value == true && "Vector element type is not copy assignable. No way to copy elements in this vector.");
				for (unsigned int i = 0; i < m_count; ++i)
				{
					newContents[i] = m_contents[i];
				}
			}

			Free(m_contents);
			m_contents = newContents;
		}

		void DestructAllElements()
		{
			if constexpr (UseTypeSafety) 
			{
				for (unsigned int i = 0; i < m_count; ++i)
				{
					m_contents[i].~T();
				}
			}
		}

		T* m_contents = reinterpret_cast<T*>(m_fixedContents);
		unsigned int m_count = 0;
		unsigned int m_capacity = FixedCapacity;
		char m_fixedContents[sizeof(void*[2]) + (FixedCapacity * sizeof(T))]{}; // By default - pad the size of the vector to 32, a predictable size with a matching natural alignment.
	};
	static_assert(sizeof(Vector<void*>) == 32);
}