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
		DataBuffer() = default;

		DataBuffer(void* data, size_t size) : Data(data), Size(size) {}

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

		[[nodiscard]] uint8_t* ReadBytes(size_t size, size_t offset)
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

	class ScopedDataBuffer
	{
	public:
		ScopedDataBuffer() = default;
		explicit ScopedDataBuffer(DataBuffer buffer) : m_Buffer(buffer) {}
		~ScopedDataBuffer() { m_Buffer.Release(); }

		ScopedDataBuffer(const ScopedDataBuffer&) = delete;
		ScopedDataBuffer(ScopedDataBuffer&& other) noexcept
		{
			m_Buffer = other.m_Buffer;

			other.m_Buffer = {};
		}

		ScopedDataBuffer& operator=(const ScopedDataBuffer&) = delete;
		ScopedDataBuffer& operator=(ScopedDataBuffer&& other) noexcept
		{
			m_Buffer.Release();
			m_Buffer = other.m_Buffer;

			other.m_Buffer = {};

			return *this;
		}
		ScopedDataBuffer& operator=(DataBuffer&& other) noexcept
		{
			m_Buffer.Release();
			m_Buffer = other;

			other = {};

			return *this;
		}

		void Allocate(size_t size)
		{
			m_Buffer.Allocate(size);
		}

		DataBuffer& GetDataBuffer() { return m_Buffer; }
		const DataBuffer& GetDataBuffer() const { return m_Buffer; }

		template<typename T>
		T& Read(size_t offset = 0)
		{
			return m_Buffer.Read<T>(offset);
		}

		[[nodiscard]] uint8_t* ReadBytes(size_t size, size_t offset)
		{
			return m_Buffer.ReadBytes(size, offset);
		}

		void Write(const void* data, size_t size, size_t offset = 0) { m_Buffer.Write(data, size, offset); }

		void Release() { m_Buffer.Release(); }

		operator bool() const
		{
			return m_Buffer;
		}

		void* Data() { return m_Buffer.Data; }
		const void* Data() const { return m_Buffer.Data; }
		size_t Size() const { return m_Buffer.Size; }

	private:
		DataBuffer m_Buffer;
	};
}
