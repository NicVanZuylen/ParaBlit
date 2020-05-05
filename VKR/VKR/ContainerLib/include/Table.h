#pragma once
#include <string>
#include "DynamicArray.h"

template<typename T>
struct HashTablePair 
{
	std::string m_key = "NO_KEY";
	T m_value;
};

template<typename T>
class Table 
{
public:

	Table() 
	{
		m_size = 1000;
		m_contents = new DynArr<HashTablePair<T>*>[m_size];
	}

	Table(const unsigned int& size)
	{
		m_size = size;
		m_contents = new DynArr<HashTablePair<T>*>[m_size];
	}

	~Table() 
	{
		for (int i = 0; i < m_contents->Count(); ++i)
			delete (*m_contents)[i];

		for (int i = 0; i < m_pairs.Count(); ++i)
			delete m_pairs[i];

		delete[] m_contents;
	}

	// Overload for retreiving data with a C string.
	inline T& operator [] (const char* data)
	{
		return Get(data, std::strlen(data));
	}

	// Overload for retreiving data with a std::string.
	inline T& operator [] (const std::string& data)
	{
		return Get(data.c_str(), data.size());
	}

	inline T& Get(const char* data, const size_t& nLength) 
	{
		unsigned int hashID = Hash(data, nLength);

		// Restrict hash range to array size.
		hashID %= m_size;

		// Get key-value pair array.
		DynArr<HashTablePair<T>*>& arr = m_contents[hashID];

		if (arr.Count() == 0)
		{
			arr.Push(CreatePair());
			arr[0]->m_key = data;

			return arr[0]->m_value;
		}

#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		bool bCollisionDetected = false;
#endif

		// If the matching pair wasn't found assign the new value or find the existing matching pair.
		for (int i = 0; i < arr.Count(); ++i)
		{
			HashTablePair<T>& pair = *arr[i];

			if (std::strcmp(data, pair.m_key.c_str()) == 0)
			{
				// Existing matching pair found.
				return pair.m_value;
			}

#ifdef CONTAINER_DEBUG_IMPLEMENTATION
			if (!bCollisionDetected)
			{
				std::cout << "Table Warning: Hash collision detected!" << std::endl;
				bCollisionDetected = true;
			}
#endif
		}

		// In case of hash collision:

		int index = arr.Count();
		arr.Push(CreatePair());
		arr[index]->m_key = data;

		return arr[index]->m_value;
	}

private:

	// BKDR Hash
	inline int Hash(const char*& data, const size_t& size)
	{
		size_t nHash = 0;

		for (size_t i = 0; i < size; ++i)
		{
			nHash = (1313ULL * nHash) + data[i];
		}

		return (nHash & 0x7FFFFFFFULL);
	}

	inline HashTablePair<T>* CreatePair() 
	{
		HashTablePair<T>* newPair = new HashTablePair<T>;
		m_pairs.Push(newPair);

		return newPair;
	}

	DynArr<HashTablePair<T>*>* m_contents;
	DynArr<HashTablePair<T>*> m_pairs;
	unsigned int m_size;
};
