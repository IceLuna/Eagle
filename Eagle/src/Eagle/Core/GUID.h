#pragma once

#define GUID_TYPE uint64_t

namespace Eagle
{
	class GUID
	{
	public:
		GUID();
		GUID(GUID_TYPE guid) : m_GUID(guid) {}
		GUID(const GUID& guid) = default;

		bool operator< (const GUID& other) const
		{
			return m_GUID < other.m_GUID;
		}

		bool operator==(const GUID& other) const
		{
			return m_GUID == other.m_GUID;
		}

		bool operator!=(const GUID& other) const
		{
			return !(*this == other);
		}

		operator GUID_TYPE() { return m_GUID; }
		operator GUID_TYPE() const { return m_GUID; }

	private:
		GUID_TYPE m_GUID;
	};
}

namespace std 
{
	template <>
	struct hash<Eagle::GUID>
	{
		std::size_t operator()(const Eagle::GUID& uuid) const
		{
			return hash<GUID_TYPE>()((GUID_TYPE)uuid);
		}
	};
}
