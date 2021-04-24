#include "egpch.h"
#include "Buffer.h"

#include "Renderer.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Eagle
{
	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;

			case RendererAPI::API::OpenGL:
				return MakeRef<OpenGLVertexBuffer>(size);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(const void* verteces, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;

			case RendererAPI::API::OpenGL:
				return MakeRef<OpenGLVertexBuffer>(verteces, size);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<IndexBuffer> IndexBuffer::Create(const uint32_t* indeces, uint32_t count)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
			return nullptr;

		case RendererAPI::API::OpenGL:
			return MakeRef<OpenGLIndexBuffer>(indeces, count);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	BufferLayout::BufferLayout(const std::initializer_list<BufferElement>& elements)
		: m_Elements(elements)
	{
		CalculateStrideAndOffset();
	}

	void BufferLayout::CalculateStrideAndOffset()
	{
		size_t offset = 0;
		m_Stride = 0;

		for (auto& element : m_Elements)
		{
			element.Offset = offset;
			offset += element.Size;
		}
		m_Stride = (uint32_t)offset;
	}
}