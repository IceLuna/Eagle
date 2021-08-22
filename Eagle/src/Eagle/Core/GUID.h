#pragma once

namespace Eagle
{
	class GUID
	{
	public:
		GUID();
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

	private:
		uint64_t m_Higher64;
		uint64_t m_Lower64;
	};
}
