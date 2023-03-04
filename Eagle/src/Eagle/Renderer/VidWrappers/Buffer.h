#pragma once

#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	struct BufferSpecifications
	{
		size_t Size = 0;
		MemoryType MemoryType = MemoryType::Gpu;
		BufferUsage Usage = BufferUsage::None;
	};

	class Buffer
	{
	protected:
		Buffer(const BufferSpecifications& specs, const std::string& debugName = "")
			: m_Specs(specs)
			, m_DebugName(debugName) {}

	public:
		virtual ~Buffer() = default;

		virtual void Resize(size_t size) = 0;

		[[nodiscard]] virtual void* Map() = 0;
		virtual void Unmap() = 0;
		virtual void* GetHandle() const = 0;

		size_t GetSize() const { return m_Specs.Size; }
		MemoryType GetMemoryType() const { return m_Specs.MemoryType; }
		BufferUsage GetUsage() const { return m_Specs.Usage; }

		bool HasUsage(BufferUsage usage) const { return HasFlags(m_Specs.Usage, usage); }

		static Ref<Buffer> Create(const BufferSpecifications& specs, const std::string& debugName = "");

		static Ref<Buffer> Dummy;

	protected:
		BufferSpecifications m_Specs;
		std::string m_DebugName;
	};
}
