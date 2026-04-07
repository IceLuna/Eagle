#pragma once
#include <typeinfo>
#include <cstring>
#include "Eagle/Core/Core.h"

namespace Eagle
{
	// Release buffer manually
	class DataBuffer
	{
	public:
		DataBuffer() = default;
		DataBuffer(size_t size)
		{
			Allocate(size);
		}

		DataBuffer(void* data, size_t size) : Data(data), Size(size) {}

		static DataBuffer Copy(const void* data, size_t size)
		{
			DataBuffer buffer;
			buffer.Allocate(size);
			memcpy(buffer.Data, data, size);
			return buffer;
		}

		static DataBuffer Copy(const DataBuffer& other)
		{
			return Copy(other.Data, other.Size);
		}

		void Allocate(size_t size)
		{
			delete[] Data;
			Data = nullptr;
			Size = size;

			if (size == 0)
				return;
			
			Data = new uint8_t[Size];
		}

		// Copies data from the old data
		void Resize(size_t newSize)
		{
			if (newSize == 0)
			{
				delete[] Data;
				Data = nullptr;
				Size = newSize;
				return;
			}

			void* newData = new uint8_t[newSize];
			if (Data)
			{
				memcpy(newData, Data, std::min(newSize, Size));
				delete[] Data;
			}

			Data = newData;
			Size = newSize;
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
			uint8_t* offseted = ((uint8_t*)Data) + offset;
			return *((T*)offseted);
		}

		template<typename T>
		const T& Read(size_t offset = 0) const
		{
			EG_CORE_ASSERT(offset <= Size, "Overflow");
			const uint8_t* offseted = ((uint8_t*)Data) + offset;
			return *((T*)offseted);
		}

		void Write(const void* data, size_t size, size_t offset = 0)
		{
			EG_CORE_ASSERT(size + offset <= Size, "Overflow");
			memcpy((uint8_t*)Data + offset, data, size);
		}

		void SetToZero()
		{
			memset(Data, 0, Size);
		}

		void WriteZero(size_t size, size_t offset = 0)
		{
			EG_CORE_ASSERT(size + offset <= Size, "Overflow");
			memset((uint8_t*)Data + offset, 0, size);
		}

		operator bool() const
		{
			return Data;
		}

	public:
		void* Data = nullptr;
		size_t Size = 0;
	};

	// Read-only
	class DataBufferView
	{
	public:
		DataBufferView() = default;
		DataBufferView(const void* data, size_t size) : Data(data), Size(size) {}

		template<typename T>
		T& Read(size_t offset = 0)
		{
			EG_CORE_ASSERT(offset <= Size, "Overflow");
			uint8_t* offseted = ((uint8_t*)Data) + offset;
			return *((T*)offseted);
		}

		template<typename T>
		const T& Read(size_t offset = 0) const
		{
			EG_CORE_ASSERT(offset <= Size, "Overflow");
			const uint8_t* offseted = ((uint8_t*)Data) + offset;
			return *((T*)offseted);
		}

	public:
		const void* Data = nullptr;
		size_t Size = 0;
	};

	class ScopedDataBuffer
	{
	public:
		ScopedDataBuffer() = default;
		ScopedDataBuffer(size_t size) : m_Buffer(size) {}
		explicit ScopedDataBuffer(DataBuffer buffer) : m_Buffer(buffer) {}
		~ScopedDataBuffer() { m_Buffer.Release(); }

		ScopedDataBuffer(const ScopedDataBuffer&) = delete;
		ScopedDataBuffer(ScopedDataBuffer&& other) noexcept
		{
			m_Buffer = other.m_Buffer;

			other.m_Buffer = {};
		}

		static ScopedDataBuffer Copy(const ScopedDataBuffer& other)
		{
			return ScopedDataBuffer(DataBuffer::Copy(other.Data(), other.Size()));
		}

		ScopedDataBuffer& operator=(const ScopedDataBuffer&) = delete;
		ScopedDataBuffer& operator=(ScopedDataBuffer&& other) noexcept
		{
			if (this == &other)
				return *this;

			m_Buffer.Release();
			m_Buffer = other.m_Buffer;

			other.m_Buffer = {};

			return *this;
		}
		ScopedDataBuffer& operator=(DataBuffer&& other) noexcept
		{
			if (&m_Buffer == &other)
				return *this;

			m_Buffer.Release();
			m_Buffer = other;

			other = {};

			return *this;
		}

		void Allocate(size_t size)
		{
			m_Buffer.Allocate(size);
		}

		void Resize(size_t newSize)
		{
			m_Buffer.Resize(newSize);
		}

		DataBuffer& GetDataBuffer() { return m_Buffer; }
		const DataBuffer& GetDataBuffer() const { return m_Buffer; }

		template<typename T>
		T& Read(size_t offset = 0)
		{
			return m_Buffer.Read<T>(offset);
		}

		template<typename T>
		const T& Read(size_t offset = 0) const
		{
			return m_Buffer.Read<T>(offset);
		}

		void Write(const void* data, size_t size, size_t offset = 0) { m_Buffer.Write(data, size, offset); }
		void SetToZero() { m_Buffer.SetToZero(); }
		void WriteZero(size_t size, size_t offset = 0) { m_Buffer.WriteZero(size, offset); }

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
