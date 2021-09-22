#pragma once

#define EG_GUID_TYPE Eagle::GUID

namespace Eagle
{
	class GUID
	{
	public:
		GUID();
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
			return std::hash<uint64_t>()(m_Higher64) ^ std::hash<uint64_t>()(m_Lower64);
		}

		bool IsNull() const { return m_Higher64 == 0 && m_Lower64 == 0; }

	private:
		uint64_t m_Higher64;
		uint64_t m_Lower64;
	};
}

namespace std 
{
	template <>
	struct hash<Eagle::GUID>
	{
		std::size_t operator()(const Eagle::GUID& uuid) const
		{
			return uuid.GetHash();
		}
	};
}
