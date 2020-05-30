#pragma once
#include <string>
#include "DynamicArray.h"

template<typename T>
struct PairTablePair
{
	std::string m_key;
	T m_value;
};

template<typename T>
struct PairTableSlot 
{
	DynamicArray<PairTablePair<T>> m_pairs;
	bool m_bCollision = false; // Whether or not a hash collision has occurred.
};

template<typename T, typename U>
class PairTable
{
public:

	PairTable() 
	{
		m_nSize = 1000ULL;
		m_slots.SetSize(m_nSize);
		m_slots.SetCount(m_nSize);
	}

	PairTable(const size_t& nSize) 
	{
		m_nSize = nSize;
		m_slots.SetSize(m_nSize);
		m_slots.SetCount(m_nSize);
	}

	// Move constructor.
	PairTable(const PairTable&& other)
	{
		m_nSize = other.m_nSize;
		m_slots = std::move(other.m_slots);
	}

	// Move assignment operator.
	PairTable& operator = (const PairTable&& other)
	{
		m_nSize = other.m_nSize;
		m_slots = std::move(other.m_slots);

		return *this;
	}

	T& operator [] (U key) 
	{
		// Get hash value.
		size_t nHash = Hash((const char*)&key, sizeof(U));

		// Wrap hash value into slot range.
		nHash %= m_nSize;

		PairTableSlot<T>& slot = m_slots[nHash];
		DynamicArray<PairTablePair<T>>& pairs = slot.m_pairs;

		// First time getting the value...
		if (slot.m_pairs.Count() == 0)
		{
			pairs.SetCount(1);

			// Wipe value memory to zero initially.
			std::memset(&(pairs[0].m_value), 0, sizeof(T));

			// Set key.
			pairs[0].m_key = std::string((const char*)&key);

			// Return new value.
			return pairs[0].m_value;
		}

		// Retreival...
		if (!slot.m_bCollision)
		{
			return pairs[0].m_value;
		}

		const char* keyString = (const char*)&key;

		// A hash collision has occured at this point, compare keys.
		for (size_t i = 0; i < pairs.Count(); ++i)
		{
			PairTablePair<T>& currentPair = pairs[i];

			if (currentPair.m_key == keyString) // Compare keys...
			{
				// This is the matching pair, return the value.
				return currentPair.m_value;
			}
		}

		// And if that still didn't get a value, a hash collision just occurred and a new value needs to be added.
		slot.m_bCollision = true;
		return AddValue(pairs, keyString);
	}

	inline T& Get(U key) 
	{
		// Get hash value.
		size_t nHash = Hash((const char*)&key, sizeof(U));

		// Wrap hash value into slot range.
		nHash %= m_nSize;

		PairTableSlot<T>& slot = m_slots[nHash];
		DynamicArray<PairTablePair<T>>& pairs = slot.m_pairs;

		// First time getting the value...
		if(slot.m_pairs.Count() == 0) 
		{
			pairs.SetCount(1ULL);

			// Wipe value memory to zero initially.
			std::memset(&(pairs[0].m_value), 0, sizeof(T));

			// Set key.
			pairs[0].m_key = (const char*)&key;

			// Return new value.
			return pairs[0].m_value;
		}

		// Retreival...
		if (!slot.m_bCollision) 
		{
			return pairs[0].m_value;
		}

		const char* keyString = (const char*)&key;

		// A hash collision has occured at this point, compare keys.
		for (size_t i = 0; i < pairs.Count(); ++i) 
		{
			PairTablePair<T>& currentPair = pairs[i];

			if(currentPair.m_key == keyString) // Compare keys...
			{
				// This is the matching pair, return the value.
				return currentPair.m_value;
			}
		}
		
		// And if that still didn't get a value, a hash collision just occurred and a new value needs to be added.
		slot.m_bCollision = true;
		return AddValue(pairs, keyString);
	}

private:

	// BKDR Hash
	inline size_t Hash(const char* data, const size_t& size)
	{
		size_t nHash = 0;

		for (size_t i = 0; i < size; ++i)
		{
			nHash = (1313ULL * nHash) + data[i];
		}

		return (nHash & 0x7FFFFFFFULL);
	}

	inline T& AddValue(DynamicArray<PairTablePair<T>>& pairs, const char* key) 
	{
		size_t nIndex = pairs.Count();

		// Add new value by incrementing the array size and setting the value count.
		pairs.SetSize(pairs.Count() + 1ULL);
		pairs.SetCount(pairs.GetSize());
		std::memset(&(pairs[nIndex].m_value), 0, sizeof(T));

		// Set key.
		pairs[nIndex].m_key = std::string(key);

		return pairs[nIndex].m_value;
	}

	DynamicArray<PairTableSlot<T>> m_slots;
	size_t m_nSize;
};
#pragma once
