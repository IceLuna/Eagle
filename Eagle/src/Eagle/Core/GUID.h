#pragma once

#include "Random.h"

namespace Eagle
{
	// 128bit GUID
	class GUID
	{
	public:
		GUID() : m_Higher64(Random::UInt64()), m_Lower64(Random::UInt64()) {}
		GUID(uint64_t high, uint64_t low) : m_Higher64(high), m_Lower64(low) {}
		GUID(const GUID& guid) = default;

		bool operator< (const GUID& other) const
		{
			if (m_Higher64 > other.m_Higher64)
				return false;

			bool bLess = m_Higher64 < other.m_Higher64;
			bLess |= m_Lower64 < other.m_Lower64;

			return bLess;
		}

		bool operator==(const GUID& other) const
		{
			return (m_Lower64 == other.m_Lower64 && m_Higher64 == other.m_Higher64);
		}

		bool operator!=(const GUID& other) const
		{
			return !(*this == other);
		}

		std::size_t GetHash() const
		{
			size_t hash = std::hash<uint64_t>()(m_Higher64);
			HashCombine(hash, m_Lower64);
			return hash;
		}

		bool IsNull() const { return m_Higher64 == 0 && m_Lower64 == 0; }

		uint64_t GetHigh() const { return m_Higher64; }
		uint64_t GetLow() const { return m_Lower64; }

	private:
		uint64_t m_Higher64;
		uint64_t m_Lower64;
	};

	// 64bit GUID
	class GUID64
	{
	public:
		GUID64() : m_ID(Random::UInt64()) {}
		GUID64(uint64_t id) : m_ID(id) {}
		GUID64(const GUID64& guid) = default;

		bool operator< (const GUID64& other) const
		{
			return m_ID < other.m_ID;
		}

		bool operator==(const GUID64& other) const
		{
			return m_ID == other.m_ID;
		}

		bool operator!=(const GUID64& other) const
		{
			return !(*this == other);
		}

		std::size_t GetHash() const
		{
			return std::hash<uint64_t>()(m_ID);
		}

		bool IsNull() const { return m_ID == 0; }

		uint64_t GetID() const { return m_ID; }

	private:
		uint64_t m_ID;
	};
}

namespace std 
{
	template <>
	struct hash<Eagle::GUID>
	{
		std::size_t operator()(const Eagle::GUID& guid) const
		{
			return guid.GetHash();
		}
	};

	template <>
	struct hash<Eagle::GUID64>
	{
		std::size_t operator()(const Eagle::GUID64& guid) const
		{
			return guid.GetHash();
		}
	};
}
