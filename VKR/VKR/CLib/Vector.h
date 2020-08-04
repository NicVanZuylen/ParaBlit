#pragma once
#include <memory>

namespace CLib
{
	// Standard Custom Vector class by Nicholas Van Zuylen. Unsafe to contain objects that own dynamic memory including other vectors.
	template<typename T, unsigned int FixedCapacity = 1, unsigned int ExpandRate = 1>
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
		}

		// Copy constructor
		Vector(const Vector<T>& other) noexcept
		{
			CopyFrom(other);
		}

		// Move constructor
		Vector(Vector<T, FixedCapacity, ExpandRate>&& other) noexcept
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
			Free(m_contents);
			m_count = 0;
			m_capacity = 0;
			m_contents = nullptr;
		}

		// ------------------------------- Read Access -------------------------------

		inline T* begin() { return m_contents; }
		inline T* end() { return &m_contents[m_count]; }
		inline T* Data() { return m_contents; }
		inline const T* Data() const { return m_contents; }
		inline T& Front() { return m_contents[0]; }
		inline T& Back() { return m_contents[m_count - 1]; }
		inline T& operator [] (const unsigned int& i) { return m_contents[i]; }
		inline const unsigned int& Count() const { return m_count; }
		inline const unsigned int& Capacity() const { return m_capacity; }

		// ------------------------------ Write Access -------------------------------

		inline T& PushBack(const T& val)
		{
			if (m_count + 1 > m_capacity)
				SetCapacity(m_count + ExpandRate);

			return m_contents[m_count++] = val;
		}

		inline T& PushBack()
		{
			if (m_count + 1 > m_capacity)
				SetCapacity(m_count + ExpandRate);

			return m_contents[m_count++];
		}

		inline T& PopBack()
		{
			if(m_count > 0)
				return m_contents[m_count-- -1];
			
			return m_contents[0];
		}

		// Requires T to have a move constructor.
		template<typename... Args>
		inline T& PushBackInit(Args&&... args)
		{
			if (m_count + 1 > m_capacity)
				SetCapacity(m_count + ExpandRate);

			return m_contents[m_count++] = std::move(T(args...));
		}

		template<typename T, unsigned int OCapacity, unsigned int OExpandRate>
		inline void Append(const Vector<T, OCapacity, OExpandRate>& other)
		{
			unsigned int newCount = m_count + other.Count();
			if (newCount > m_capacity)
			{
				m_capacity = newCount;
				SetCapacity(m_capacity);
			}

			std::memcpy(&m_contents[m_count], other.Data(), sizeof(T) * other.Count());
			m_count = newCount;
		}

		inline void Reserve(const unsigned int& newCapacity) { SetCapacity(newCapacity); }
		inline void SetCount(const unsigned int& newCount) { m_count = newCount; }
		inline void Clear() { m_count = 0; }

		// ---------------------------- Write Operators -------------------------------

		// Copy assignment
		//template<typename T, unsigned int OCapacity, unsigned int OExpandRate>
		inline Vector<T, FixedCapacity, ExpandRate>& operator = (const Vector<T, FixedCapacity, ExpandRate>& other)
		{
			CopyFrom(other);
			return *this;
		}

		// Move assignment
		inline Vector<T, FixedCapacity, ExpandRate>& operator = (Vector<T, FixedCapacity, ExpandRate>&& other)
		{
			MoveFrom(other);
			return *this;
		}

		// Initializer list assignment
		inline Vector<T, FixedCapacity, ExpandRate>& operator = (const std::initializer_list<T>& list)
		{
			CopyFromInitList(list);
			return *this;
		}

		// Append operator
		template<typename T, unsigned int OCapacity, unsigned int OExpandRate>
		inline Vector<T, FixedCapacity, ExpandRate>& operator += (const Vector<T, OCapacity, OExpandRate>& other)
		{
			Append(other);
			return *this;
		}

	private:

		template<typename T, unsigned int OCapacity, unsigned int OExpandRate>
		inline void CopyFrom(const Vector<T, OCapacity, OExpandRate>& other)
		{
			m_count = other.Count();
			m_capacity = m_count;
			m_contents = Alloc(m_capacity);
			memcpy(m_contents, other.Data(), sizeof(T) * m_count);
		}

		inline void MoveFrom(Vector<T, FixedCapacity, ExpandRate>& other)
		{
			m_count = other.m_count;
			if (other.m_contents != reinterpret_cast<T*>(other.m_fixedContents))
			{
				m_contents = other.m_contents;
				m_capacity = other.m_capacity;
			}
			else if(other.m_count <= FixedCapacity)
			{
				m_contents = reinterpret_cast<T*>(m_fixedContents);
				memcpy(m_contents, other.m_contents, sizeof(T) * m_count);
			}
			else
			{
				m_capacity = m_count;
				m_contents = Alloc(m_capacity);
			}

			other.m_capacity = 0;
			other.m_count = 0;
			other.m_contents = nullptr;
		}

		inline void CopyFromInitList(const std::initializer_list<T>& list)
		{
			m_count = static_cast<unsigned int>(list.size());
			m_capacity = m_count;
			m_contents = Alloc(m_capacity);

			// Copy list contents.
			memcpy(m_contents, list.begin(), sizeof(T) * m_count);
		}

		inline T* Alloc(const unsigned int& capacity)
		{
			return new T[capacity];
		}

		inline void Free(T* mem)
		{
			if (mem != reinterpret_cast<T*>(m_fixedContents) && mem)
			{
				delete mem;
			}
		}

		inline void SetCapacity(const unsigned int& newCapacity)
		{
			// Nothing to be done if the new capacity is within the capacity of the fixed contents, and we're already using the fixed contents.
			if (newCapacity <= FixedCapacity && m_contents == reinterpret_cast<T*>(m_fixedContents))
				return;

			// Use fixed contents if the new capacity fits within the fixed capacity.
			T* newContents = reinterpret_cast<T*>(m_fixedContents);
			if (newCapacity <= FixedCapacity)
				m_capacity = FixedCapacity;
			else
			{
				m_capacity = newCapacity;
				newContents = Alloc(m_capacity);
			}

			if (m_count > m_capacity) // Trim old contents.
				m_count = m_capacity;

			memcpy(newContents, m_contents, sizeof(T) * m_count);

			Free(m_contents);
			m_contents = newContents;
		}

		T* m_contents = reinterpret_cast<T*>(m_fixedContents);
		unsigned int m_count = 0;
		unsigned int m_capacity = FixedCapacity;
		char m_fixedContents[sizeof(void*) + (FixedCapacity * sizeof(T))]{};
	};
}