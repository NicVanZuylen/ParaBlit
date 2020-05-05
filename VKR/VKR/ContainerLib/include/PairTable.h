#pragma once
#include <string>
#include "DynamicArray.h"

template<typename T>
struct PairTablePair
{
	std::string m_key = "NO_KEY";
	T m_value;
};

template<typename T, typename U>
class PairTable
{
public:

	PairTable()
	{
		m_nSize = 1000;
		m_contents = new DynArr<PairTablePair<T>*>[m_nSize];
	}

	PairTable(size_t nSize)
	{
		m_nSize = nSize;
		m_contents = new DynArr<PairTablePair<T>*>[m_nSize];
	}

	~PairTable()
	{
		for (int i = 0; i < m_pairs.Count(); ++i)
			delete m_pairs[i];

		delete[] m_contents;
	}

	T& operator [] (U key)
	{
		const char* data = (const char*)&key;
		size_t nSize = sizeof(U);

		size_t nHashID = Hash(data, nSize);

		// Restrict hash range to array size.
		nHashID %= m_nSize;

		// Get key-value pair array.
		DynArr<PairTablePair<T>*>& arr = m_contents[nHashID];

		if (arr.Count() == 0)
		{
			size_t nIndex = arr.Count();
			arr.Push(CreatePair());
			arr[nIndex]->m_key = data;

			return arr[nIndex]->m_value;
		}

#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		bool bCollisionDetected = false;
#endif

		// If the matching pair wasnt found assign the new value or find the existing matching pair.
		for (size_t i = 0; i < arr.Count(); ++i)
		{
			PairTablePair<T>& pair = *arr[i];
			const char* cKey = pair.m_key.c_str();

			if (strcmp(data, pair.m_key.c_str()) == 0)
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

		size_t nIndex = arr.Count();
		arr.Push(CreatePair());
		arr[nIndex]->m_key = data;

		return arr[nIndex]->m_value;
	}

private:

	// BKDR Hash
	inline size_t Hash(const char*& data, const size_t& nSize)
	{
		size_t nHash = 0;

		for (size_t i = 0; i < nSize; ++i)
		{
			nHash = (1313 * nHash) + data[i];
		}

		return (nHash & 0x7FFFFFFF);
	}

	inline PairTablePair<T>* CreatePair()
	{
		PairTablePair<T>* newPair = new PairTablePair<T>;
		m_pairs.Push(newPair);

		return newPair;
	}

	DynArr<PairTablePair<T>*>* m_contents;
	DynArr<PairTablePair<T>*> m_pairs;
	size_t m_nSize;
};
#pragma once
