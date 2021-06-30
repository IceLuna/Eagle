#include "egpch.h"
#include "OpenGLBuffer.h"

#include <glad/glad.h>
#include "Eagle/Renderer/Shader.h"

namespace Eagle
{
	/////////////////////////////////////
	//////	OpenGL VertexBuffer /////////
	/////////////////////////////////////

	OpenGLVertexBuffer::OpenGLVertexBuffer()
	{
		glCreateBuffers(1, &m_RendererID);
	}

	OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGLVertexBuffer::OpenGLVertexBuffer(const void* verteces, uint32_t size)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, verteces, GL_STATIC_DRAW);
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLVertexBuffer::Bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLVertexBuffer::Unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void OpenGLVertexBuffer::UpdateData(const void* data, uint32_t size)
	{
		glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
	}

	void OpenGLVertexBuffer::SetData(const void* data, uint32_t size)
	{
		glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);
	}

	/////////////////////////////////////
	//////	OpenGL IndexBuffer //////////
	/////////////////////////////////////

	OpenGLIndexBuffer::OpenGLIndexBuffer()
	{
		glCreateBuffers(1, &m_RendererID);
	}

	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t count) 
	: m_Count(count)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), nullptr, GL_STATIC_DRAW);
	}

	OpenGLIndexBuffer::OpenGLIndexBuffer(const uint32_t* indeces, uint32_t count)
	: m_Count(count)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indeces, GL_STATIC_DRAW);
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLIndexBuffer::Bind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLIndexBuffer::Unbind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void OpenGLIndexBuffer::SetData(const void* indeces, uint32_t count)
	{
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indeces, GL_STATIC_DRAW);
	}

	/////////////////////////////////////
	//////	OpenGL UniformBuffer ////////
	/////////////////////////////////////

	OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t bindingSlot)
	: m_BindingSlot(bindingSlot)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBufferBase(GL_UNIFORM_BUFFER, m_BindingSlot, m_RendererID);
	}

	OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size, uint32_t bindingSlot)
	: m_BindingSlot(bindingSlot)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, m_BindingSlot, m_RendererID);
	}

	OpenGLUniformBuffer::OpenGLUniformBuffer(const void* data, uint32_t size, uint32_t bindingSlot)
	: m_BindingSlot(bindingSlot)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, m_BindingSlot, m_RendererID);
	}

	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLUniformBuffer::Bind() const
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
	}

	void OpenGLUniformBuffer::Unbind() const
	{
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void OpenGLUniformBuffer::SetData(const void* data, uint32_t size)
	{
		glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
	}

	uint32_t OpenGLUniformBuffer::GetBlockSize(const std::string& blockName, const Ref<Shader>& inShader)
	{
		uint32_t shaderID = inShader->GetID();
		GLuint blockIndex = glGetUniformBlockIndex(shaderID, blockName.c_str());
		GLint blockSize;
		glGetActiveUniformBlockiv(shaderID, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

		return (uint32_t)blockSize;
	}

	void OpenGLUniformBuffer::UpdateData(const void* data, uint32_t size, uint32_t offset)
	{
		glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
	}

}