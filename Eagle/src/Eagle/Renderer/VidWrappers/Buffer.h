#pragma once

#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	struct BufferSpecifications
	{
		size_t Size = 0;
		MemoryType MemoryType = MemoryType::Gpu;
		BufferUsage Usage = BufferUsage::None;
		mutable BufferLayout Layout = BufferLayoutType::Unknown;
		ImageFormat Format = ImageFormat::Unknown; // Used if buffer usage is `StorageTexelBuffer` or `UniformTexelBuffer`
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
		virtual void* GetViewHandle() const = 0;

		size_t GetSize() const { return m_Specs.Size; }
		MemoryType GetMemoryType() const { return m_Specs.MemoryType; }
		BufferUsage GetUsage() const { return m_Specs.Usage; }
		BufferLayout GetLayout() const { return m_Specs.Layout; }

		bool HasUsage(BufferUsage usage) const { return HasFlags(m_Specs.Usage, usage); }

		static Ref<Buffer> Create(const BufferSpecifications& specs, const std::string& debugName = "");

		static Ref<Buffer> Dummy;

	private:
		void SetLayout(BufferLayout layout) const { m_Specs.Layout = layout; }

	protected:
		BufferSpecifications m_Specs;
		std::string m_DebugName;

		friend class VulkanCommandManager;
		friend class VulkanCommandBuffer;
	};
}
