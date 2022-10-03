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
		static DataBuffer Copy(const void* data, size_t size)
		{
			DataBuffer buffer;
			buffer.Allocate(size);
			memcpy(buffer.Data, data, size);
			return buffer;
		}

		void Allocate(size_t size)
		{
			delete[] Data;
			Data = nullptr;
			
			if (size == 0)
				return;
			
			Size = size;
			Data = new uint8_t[Size];
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		template<typename T>
		T& Read(size_t offset = 0)
		{
			EG_CORE_ASSERT(offset <= Size, "Overflow");
			return *((T*)((uint8_t*)Data) + offset);
		}

		uint8_t* ReadBytes(size_t size, size_t offset)
		{
			EG_CORE_ASSERT(size + offset <= Size, "Overflow");
			uint8_t* buffer = new uint8_t[size];
			memcpy(buffer, (uint8_t*)Data + offset, size);
			return buffer;
		}

		void Write(const void* data, size_t size, size_t offset = 0)
		{
			EG_CORE_ASSERT(size + offset <= Size, "Overflow");
			memcpy((uint8_t*)Data + offset, data, size);
		}

		operator bool() const
		{
			return Data;
		}

	public:
		void* Data = nullptr;
		size_t Size = 0;
	};
}
