#pragma once
#include <typeinfo>
#include <cstring>
#include "Eagle/Core/Core.h"

namespace Eagle
{
	//Release buffer manually
	class DataBuffer
	{
	public:

		uint32_t GetSize() const { return m_Size; }
		void* GetData() { return m_Data; }
		const void* GetData() const { return m_Data; }

		static DataBuffer Copy(const void* data, uint32_t size)
		{
			DataBuffer buffer;
			buffer.Allocate(size);
			memcpy(buffer.m_Data, data, size);
			return buffer;
		}

		void Allocate(uint32_t size)
		{
			delete[] m_Data;
			m_Data = nullptr;
			
			if (size == 0)
				return;
			
			m_Size = size;
			m_Data = new uint8_t[m_Size];
		}

		void Release()
		{
			delete[] m_Data;
			m_Data = nullptr;
			m_Size = 0;
		}

		template<typename T>
		T& Read(uint32_t offset = 0)
		{
			EG_CORE_ASSERT(offset <= m_Size, "Overflow");
			return *((T*)((uint8_t*)m_Data) + offset);
		}

		uint8_t* ReadBytes(uint32_t size, uint32_t offset)
		{
			EG_CORE_ASSERT(size + offset <= m_Size, "Overflow");
			uint8_t* buffer = new uint8_t[size];
			memcpy(buffer, (uint8_t*)m_Data + offset, size);
			return buffer;
		}

		void Write(const void* data, uint32_t size, uint32_t offset = 0)
		{
			EG_CORE_ASSERT(size + offset <= m_Size, "Overflow");
			memcpy((uint8_t*)m_Data + offset, data, size);
		}

		operator bool() const
		{
			return m_Data;
		}

	private:
		void* m_Data = nullptr;
		uint32_t m_Size = 0;
	};
}
