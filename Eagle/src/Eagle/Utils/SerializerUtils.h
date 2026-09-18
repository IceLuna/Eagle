#pragma once

#include "Compressor.h"
#include "Eagle/Utils/YamlUtils.h"
#include "Eagle/Core/DataBuffer.h"

namespace Eagle
{
	// Note: YAML size is not stored here, since it's written at the end of the file (we read till the end of the file).
	// This way we can easily modify YAML in any text editor without worrying about any alignments issues
	struct AssetHeader
	{
		size_t OffsetToYaml = 0;
	};
	static_assert(std::is_trivially_copyable_v<AssetHeader>);

	namespace Utils
	{
		// Returns true if [offset, offset + size) lies within `data`. Overflow-safe
		inline bool IsRangeValid(const DataBuffer& data, size_t offset, size_t size)
		{
			return offset <= data.Size && size <= data.Size - offset;
		}

		// Adds size to `outTotalSize`.
		// Returns old `outTotalSize` value. Can be used to save an offset
		inline size_t AddSize(const YAML::Emitter& emitter, size_t* outTotalSize)
		{
			const size_t oldSize = *outTotalSize;
			*outTotalSize += emitter.size() + 1; // +1 for '\0'
			return oldSize;
		}

		// Adds size to `outTotalSize`.
		// Returns old `outTotalSize` value. Can be used to save an offset
		inline size_t AddSize(const std::string& str, size_t* outTotalSize)
		{
			const size_t oldSize = *outTotalSize;
			*outTotalSize += str.size() + 1; // +1 for '\0'
			return oldSize;
		}

		// Adds size to `outTotalSize`.
		// Returns old `outTotalSize` value. Can be used to save an offset
		inline size_t AddSize(const ScopedDataBuffer& data, size_t* outTotalSize)
		{
			const size_t oldSize = *outTotalSize;
			*outTotalSize += data.Size();
			return oldSize;
		}

		// Adds size to `outTotalSize`.
		// Returns old `outTotalSize` value. Can be used to save an offset
		inline size_t AddSize(const DataBuffer& data, size_t* outTotalSize)
		{
			const size_t oldSize = *outTotalSize;
			*outTotalSize += data.Size;
			return oldSize;
		}

		// Adds size to `outTotalSize`.
		// Returns old `outTotalSize` value. Can be used to save an offset
		template<typename T>
		inline size_t AddSize(const std::vector<T>& data, size_t* outTotalSize)
		{
			const size_t oldSize = *outTotalSize;
			*outTotalSize += data.size() * sizeof(T);
			return oldSize;
		}

		// Adds size to `outTotalSize`.
		// Returns old `outTotalSize` value. Can be used to save an offset
		inline size_t AddSize(size_t size, size_t* outTotalSize)
		{
			const size_t oldSize = *outTotalSize;
			*outTotalSize += size;
			return oldSize;
		}

		// Creates a header and updates `outTotalSize`
		inline AssetHeader CreateHeader(const YAML::Emitter& emitter, size_t* outTotalSize)
		{
			AssetHeader header;
			header.OffsetToYaml = AddSize(emitter, outTotalSize);
			return header;
		}

		// Creates a header and updates `outTotalSize`
		inline AssetHeader CreateHeader(const std::string& yamlDescription, size_t* outTotalSize)
		{
			AssetHeader header;
			header.OffsetToYaml = AddSize(yamlDescription, outTotalSize);
			return header;
		}

		// Note: these `Write` functions don't resize the `dst` buffer.
		// Returns old `offset` value
		template<typename BufferType>
		inline size_t WriteToBuffer(BufferType& dst, const DataBuffer& src, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			if (src.Size == 0)
				return *outOffset;

			const size_t oldOffset = *outOffset;
			dst.Write(src.Data, src.Size, *outOffset);
			*outOffset += src.Size;
			return oldOffset;
		}

		template<typename BufferType>
		inline size_t WriteToBuffer(BufferType& dst, const ScopedDataBuffer& src, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			return WriteToBuffer(dst, src.GetDataBuffer(), outOffset);
		}

		template<typename BufferType>
		inline size_t WriteToBuffer(BufferType& dst, const void* src, size_t srcSize, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			if (srcSize == 0)
				return *outOffset;

			const size_t oldOffset = *outOffset;
			dst.Write(src, srcSize, *outOffset);
			*outOffset += srcSize;
			return oldOffset;
		}

		template <typename BufferType, typename T>
		inline size_t WriteToBuffer(BufferType& dst, const std::vector<T>& src, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			static_assert(std::is_trivially_copyable_v<T>, "Only trivially copyable types can be written as raw bytes");
			if (src.empty())
				return *outOffset;

			const size_t srcSize = src.size() * sizeof(T);
			const size_t oldOffset = *outOffset;
			dst.Write(src.data(), srcSize, *outOffset);
			*outOffset += srcSize;
			return oldOffset;
		}

		template<typename BufferType>
		inline size_t WriteToBuffer(BufferType& dst, uint8_t val, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			const size_t oldOffset = *outOffset;
			dst.Write(&val, sizeof(val), *outOffset);
			*outOffset += sizeof(val);
			return oldOffset;
		}

		template<typename BufferType>
		inline size_t WriteToBuffer(BufferType& dst, uint32_t val, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			const size_t oldOffset = *outOffset;
			dst.Write(&val, sizeof(val), *outOffset);
			*outOffset += sizeof(val);
			return oldOffset;
		}

		template<typename BufferType>
		inline size_t WriteToBuffer(BufferType& dst, uint64_t val, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			const size_t oldOffset = *outOffset;
			dst.Write(&val, sizeof(val), *outOffset);
			*outOffset += sizeof(val);
			return oldOffset;
		}

		template<typename BufferType>
		inline size_t WriteStringToBuffer(BufferType& dst, const char* str, size_t size, size_t* outOffset)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			const size_t oldOffset = *outOffset;
			Utils::WriteToBuffer(dst, str, size, outOffset);
			Utils::WriteToBuffer(dst, uint8_t(0), outOffset);
			return oldOffset;
		}

		template<typename BufferType>
		inline size_t WriteStringToBuffer(BufferType& dst, const std::string& str, size_t* outOffset)
		{
			return WriteStringToBuffer(dst, str.c_str(), str.size(), outOffset);
		}

		template<typename BufferType>
		inline size_t WriteYaml(BufferType& dst, const YAML::Emitter& yaml, size_t* outOffset)
		{
			return Utils::WriteStringToBuffer(dst, yaml.c_str(), yaml.size(), outOffset);
		}

		template<typename BufferType>
		inline AssetHeader ReadHeader(const BufferType& data)
		{
			static_assert(std::is_same_v<BufferType, DataBuffer> || std::is_same_v<BufferType, ScopedDataBuffer>);
			return data.Read<AssetHeader>();
		}

		inline void ReadYAML(const DataBuffer& data, YAML::Node* outYaml)
		{
			const AssetHeader header = ReadHeader(data);
			const uint8_t* yamlData = (const uint8_t*)data.Data + header.OffsetToYaml;

			*outYaml = YAML::Load((const char*)yamlData);
		}

		inline void ReadYAML(const ScopedDataBuffer& data, YAML::Node* outYaml)
		{
			ReadYAML(data.GetDataBuffer(), outYaml);
		}

		static inline std::string ReadYAMLToString(const DataBuffer& data)
		{
			const AssetHeader header = ReadHeader(data);
			const uint8_t* yamlData = (const uint8_t*)data.Data + header.OffsetToYaml;

			return std::string((const char*)yamlData);
		}

		inline std::string ReadYAMLToString(const ScopedDataBuffer& data)
		{
			return ReadYAMLToString(data.GetDataBuffer());
		}

		inline const char* ReadYAMLAsCString(const DataBuffer& data)
		{
			const AssetHeader header = ReadHeader(data);
			const uint8_t* yamlData = (const uint8_t*)data.Data + header.OffsetToYaml;
			return (const char*)yamlData;
		}

		// All `ReadBinary` / `ReadCompressedBinary` functions validate the requested range and
		// return false (leaving the output empty) if the data is invalid
		inline bool ReadBinary(const DataBuffer& data, size_t size, size_t offset, DataBuffer* outData)
		{
			if (!IsRangeValid(data, offset, size))
			{
				EG_CORE_ERROR("Failed to read binary data. Range [{}, {} + {}) is out of bounds. Data size: {}", offset, offset, size, data.Size);
				return false;
			}

			outData->Allocate(size);
			memcpy(outData->Data, (const uint8_t*)data.Data + offset, size);
			return true;
		}

		inline bool ReadBinary(const DataBuffer& data, size_t size, size_t offset, ScopedDataBuffer* outData)
		{
			return ReadBinary(data, size, offset, &outData->GetDataBuffer());
		}

		template<typename T>
		inline bool ReadBinary(const DataBuffer& data, size_t size, size_t offset, std::vector<T>* outData)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Only trivially copyable types can be read as raw bytes");
			outData->clear();

			if (!IsRangeValid(data, offset, size))
			{
				EG_CORE_ERROR("Failed to read binary data. Range [{}, {} + {}) is out of bounds. Data size: {}", offset, offset, size, data.Size);
				return false;
			}
			if (size % sizeof(T) != 0)
			{
				EG_CORE_ERROR("Failed to read binary data. Size ({}) is not a multiple of the element size ({})", size, sizeof(T));
				return false;
			}

			const size_t count = size / sizeof(T);
			outData->resize(count);
			memcpy(outData->data(), (const uint8_t*)data.Data + offset, size);
			return true;
		}

		inline bool ReadCompressedBinary(const DataBuffer& data, size_t size, size_t offset, ScopedDataBuffer* outData)
		{
			*outData = ScopedDataBuffer();

			if (!IsRangeValid(data, offset, size))
			{
				EG_CORE_ERROR("Failed to read compressed data. Range [{}, {} + {}) is out of bounds. Data size: {}", offset, offset, size, data.Size);
				return false;
			}

			const DataBuffer requestedData = DataBuffer((uint8_t*)data.Data + offset, size);
			*outData = Compressor::Decompress(requestedData);
			return outData->Size() > 0;
		}

		template<typename T>
		inline bool ReadCompressedBinary(const DataBuffer& data, size_t size, size_t offset, std::vector<T>* outData)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Only trivially copyable types can be read as raw bytes");
			outData->clear();

			if (!IsRangeValid(data, offset, size))
			{
				EG_CORE_ERROR("Failed to read compressed data. Range [{}, {} + {}) is out of bounds. Data size: {}", offset, offset, size, data.Size);
				return false;
			}
			const DataBuffer requestedData = DataBuffer((uint8_t*)data.Data + offset, size);
			ScopedDataBuffer decompressed = Compressor::Decompress(requestedData);

			if ((decompressed.Size() % sizeof(T)) != 0)
			{
				EG_CORE_ERROR("Failed to read compressed data. Size ({}) is not a multiple of the element size ({})", decompressed.Size(), sizeof(T));
				return false;
			}
			const size_t count = decompressed.Size() / sizeof(T);
			outData->resize(count);

			memcpy(outData->data(), decompressed.Data(), decompressed.Size());
			return true;
		}
	}
}
