#pragma once

extern "C" 
{
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoClassField MonoClassField;
	typedef struct _MonoProperty MonoProperty;
	typedef struct _MonoType MonoType;
}

namespace Eagle
{
	struct EntityInstance;

	//Add new type to Scene Serializer
	enum class FieldType : uint32_t
	{
		None, Int, UnsignedInt, Float, String, Vec2, Vec3, Vec4, ClassReference
	};

	class PublicField
	{
	public:
		std::string Name;
		std::string TypeName;
		FieldType Type;
		bool IsReadOnly = false;

		PublicField() = default;
		PublicField(const std::string& name, const std::string& typeName, FieldType type, bool isReadOnly = false);
		PublicField(const PublicField& other);
		PublicField(PublicField&& other) noexcept;
		~PublicField();

		PublicField& operator= (const PublicField& other);
		PublicField& operator= (PublicField&& other) noexcept;

		void CopyStoredValueFromRuntime(EntityInstance& entityInstance);
		void CopyStoredValueToRuntime(EntityInstance& entityInstance);

		template<typename T>
		T GetStoredValue() const
		{
			T value;
			GetStoredValue_Internal(&value);
			return value;
		}

		template <typename T>
		void SetStoredValue(const T& value)
		{
			SetStoredValue_Internal(&value);
		}

		template <>
		void SetStoredValue(const std::string& value)
		{
			(*(std::string*)(m_StoredValueBuffer)).assign(value);
		}

		template<>
		const std::string& GetStoredValue() const
		{
			return *(std::string*)m_StoredValueBuffer;
		}

		template <typename T>
		T GetRuntimeValue(EntityInstance& entityInstance) const
		{
			T value;
			GetRuntimeValue_Internal(entityInstance, &value);
			return value;
		}

		template <>
		std::string GetRuntimeValue(EntityInstance& entityInstance) const
		{
			std::string value;
			GetRuntimeValue_Internal(entityInstance, value);
			return value;
		}

		template <typename T>
		void SetRuntimeValue(EntityInstance& entityInstance, T& value)
		{
			SetRuntimeValue_Internal(entityInstance, &value);
		}

		template <>
		void SetRuntimeValue(EntityInstance& entityInstance, const std::string& value)
		{
			SetRuntimeValue_Internal(entityInstance, value);
		}

		static uint32_t GetFieldSize(FieldType type)
		{
			switch (type)
			{
			case FieldType::Int: return 4;
			case FieldType::UnsignedInt: return 4;
			case FieldType::Float: return 4;
			case FieldType::String: return 8;
			case FieldType::Vec2: return 4 * 2;
			case FieldType::Vec3: return 4 * 3;
			case FieldType::Vec4: return 4 * 4;
			}
			EG_CORE_ASSERT(false, "Unknown type size");
			return 0;
		}

	private:
		void GetStoredValue_Internal(void* outValue) const
		{
			uint32_t size = GetFieldSize(Type);
			memcpy(outValue, m_StoredValueBuffer, size);
		}

		void SetStoredValue_Internal(const void* value)
		{
			if (IsReadOnly)
				return;

			uint32_t size = GetFieldSize(Type);
			memcpy(m_StoredValueBuffer, value, size);
		}

		void SetRuntimeValue_Internal(EntityInstance& entityInstance, void* value);
		void SetRuntimeValue_Internal(EntityInstance& entityInstance, const std::string& value);
		void GetRuntimeValue_Internal(EntityInstance& entityInstance, void* outValue) const;
		void GetRuntimeValue_Internal(EntityInstance& entityInstance, std::string& outValue) const;

		uint8_t* AllocateBuffer(FieldType type)
		{
			uint32_t size = GetFieldSize(type);
			uint8_t* buffer = new uint8_t[size];
			memset(buffer, 0, size);
			return buffer;
		}

	private:
		MonoClassField* m_MonoClassField = nullptr;
		MonoProperty* m_MonoProperty = nullptr;
		uint8_t* m_StoredValueBuffer = nullptr;

		friend class ScriptEngine;
	};
}