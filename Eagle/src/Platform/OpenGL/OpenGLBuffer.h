#pragma once

#include "Eagle/Renderer/Buffer.h"

namespace Eagle
{
	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer();
		OpenGLVertexBuffer(uint32_t size);
		OpenGLVertexBuffer(const void* verteces, uint32_t size);
		virtual ~OpenGLVertexBuffer() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void UpdateData(const void* data, uint32_t size, uint32_t offset = 0) override;
		virtual void SetData(const void* data, uint32_t size) override;

		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
		virtual const BufferLayout& GetLayout() const override { return m_Layout; }

	private:
		uint32_t m_RendererID = 0;
		BufferLayout m_Layout;
	};

	class OpenGLIndexBuffer : public IndexBuffer
	{
	public:
		OpenGLIndexBuffer();
		OpenGLIndexBuffer(uint32_t count);
		OpenGLIndexBuffer(const uint32_t* indeces, uint32_t count);
		virtual ~OpenGLIndexBuffer() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void SetData(const void* indeces, uint32_t count) override;
		virtual void UpdateData(const void* indeces, uint32_t count, uint32_t offset = 0) override;

		virtual uint32_t GetCount() const override { return m_Count; }

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Count = 0;
	};

	class OpenGLUniformBuffer : public UniformBuffer
	{
	public:
		OpenGLUniformBuffer(uint32_t bindingSlot);
		OpenGLUniformBuffer(uint32_t size, uint32_t bindingSlot);
		OpenGLUniformBuffer(const void* data, uint32_t size, uint32_t bindingSlot);
		virtual ~OpenGLUniformBuffer() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		//If default constructor was used, call this function to init memory.
		virtual void SetData(const void* data, uint32_t size) override;
		virtual uint32_t GetBlockSize(const std::string& blockName, const Ref<Shader>& inShader) override;

		virtual void UpdateData(const void* data, uint32_t size, uint32_t offset) override;

		virtual uint32_t GetBindedSlot() const override { return m_BindingSlot; }

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_BindingSlot = 0;
	};
}