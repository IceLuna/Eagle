#pragma once

namespace Eagle
{
	enum class ShaderDataType
	{
		None = 0,
		Float, Float2, Float3, Float4,
		Mat3, Mat4,
		Int, Int2, Int3, Int4,
		Bool
	};

	static uint32_t ShaderDataTypeSize(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float:		return sizeof(float);
			case ShaderDataType::Float2:	return sizeof(float) * 2;
			case ShaderDataType::Float3:	return sizeof(float) * 3;
			case ShaderDataType::Float4:	return sizeof(float) * 4;
			case ShaderDataType::Mat3:		return sizeof(float) * 3 * 3;
			case ShaderDataType::Mat4:		return sizeof(float) * 4 * 4;
			case ShaderDataType::Int:		return sizeof(int);
			case ShaderDataType::Int2:		return sizeof(int) * 2;
			case ShaderDataType::Int3:		return sizeof(int) * 3;
			case ShaderDataType::Int4:		return sizeof(int) * 4;
			case ShaderDataType::Bool:		return sizeof(bool);
		}

		EG_CORE_ASSERT(false, "Unknow ShaderDataType!");
		return 0;
	}

	struct BufferElement
	{
		ShaderDataType Type;
		std::string Name;
		bool Normalized;
		uint32_t Offset;
		uint32_t Size;

		BufferElement() : 
			Type(ShaderDataType::None),
			Name(),
			Normalized(false),
			Offset(0),
			Size(0)
		{}

		BufferElement(ShaderDataType type, const std::string& name, bool normalized = false) :
			Type(type),
			Name(name),
			Normalized(normalized),
			Offset(0),
			Size(ShaderDataTypeSize(type))
		{}

		inline uint32_t GetOffset() const { return Offset; }

		uint32_t GetComponentCount() const
		{
			switch (Type)
			{
				case ShaderDataType::Float:		return 1;
				case ShaderDataType::Float2:	return 2;
				case ShaderDataType::Float3:	return 3;
				case ShaderDataType::Float4:	return 4;
				case ShaderDataType::Mat3:		return 3 * 3;
				case ShaderDataType::Mat4:		return 4 * 4;
				case ShaderDataType::Int:		return 1;
				case ShaderDataType::Int2:		return 2;
				case ShaderDataType::Int3:		return 3;
				case ShaderDataType::Int4:		return 4;
				case ShaderDataType::Bool:		return 1;
			}

			EG_CORE_ASSERT(false, "Unknow ShaderDataType!");
			return 0;
		}
	};
	
	class BufferLayout
	{
		using BufferLayoutVector = std::vector<BufferElement>;

	public:
		BufferLayout() = default;
		BufferLayout(const std::initializer_list<BufferElement>& elements);

		inline uint32_t GetStride() const { return m_Stride; }
		inline const std::vector<BufferElement>& GetElements() const { return m_Elements; }

		BufferLayoutVector::iterator				begin()			{ return m_Elements.begin();  }
		BufferLayoutVector::iterator				end()			{ return m_Elements.end();	  }
		BufferLayoutVector::reverse_iterator		rbegin()		{ return m_Elements.rbegin(); }
		BufferLayoutVector::reverse_iterator		rend()			{ return m_Elements.rend();   }

		BufferLayoutVector::const_iterator			begin()	  const { return m_Elements.begin();  }
		BufferLayoutVector::const_iterator			end()	  const { return m_Elements.end();	  }
		BufferLayoutVector::const_reverse_iterator rbegin()	  const { return m_Elements.rbegin(); }
		BufferLayoutVector::const_reverse_iterator rend()	  const { return m_Elements.rend();   }

	private:
		void CalculateStrideAndOffset();
	
	private:
		std::vector<BufferElement> m_Elements;
		uint32_t m_Stride = 0;
	};

	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetData(const void* data, uint32_t size) = 0;

		virtual void SetLayout(const BufferLayout& layout) = 0;
		virtual const BufferLayout& GetLayout() const = 0;

		static Ref<VertexBuffer> Create(uint32_t size);
		static Ref<VertexBuffer> Create(float* verteces, uint32_t size);
	};

	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetCount() const = 0;

		static Ref<IndexBuffer> Create(uint32_t* indeces, uint32_t count);

	};
}