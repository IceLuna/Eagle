#include "egpch.h"
#include "PublicField.h"
#include <mono/jit/jit.h>
#include "ScriptEngine.h"

namespace Eagle
{
	static std::unordered_map<std::string, std::string> s_PublicFieldStringValues;

	PublicField::PublicField(const std::string& name, const std::string& typeName, FieldType type, bool isReadOnly)
	: Name(name), TypeName(typeName), Type(type), IsReadOnly(isReadOnly)
	{
		if (Type != FieldType::String)
			m_StoredValueBuffer = AllocateBuffer(Type);
	}

	PublicField::PublicField(const PublicField& other)
		: Name(other.Name), TypeName(other.TypeName), Type(other.Type), IsReadOnly(other.IsReadOnly)
		, m_MonoClassField(other.m_MonoClassField)
		, m_MonoProperty(other.m_MonoProperty)
	{
		if (Type != FieldType::String)
		{
			m_StoredValueBuffer = AllocateBuffer(Type);
			memcpy(m_StoredValueBuffer, other.m_StoredValueBuffer, GetFieldSize(Type));
		}
		else
		{
			m_StoredValueBuffer = other.m_StoredValueBuffer;
		}
	}

	PublicField::PublicField(PublicField&& other) noexcept
		: Name(std::move(other.Name)), TypeName(std::move(other.TypeName))
		, Type(std::move(other.Type)), IsReadOnly(std::move(other.IsReadOnly))
		, m_MonoClassField(std::move(other.m_MonoClassField))
		, m_MonoProperty(std::move(other.m_MonoProperty))
	{
		m_StoredValueBuffer = other.m_StoredValueBuffer;
		other.m_StoredValueBuffer = nullptr;
	}

	PublicField::~PublicField()
	{
		if (Type != FieldType::String)
			delete[] m_StoredValueBuffer;
	}

	PublicField& PublicField::operator=(const PublicField& other)
	{
		if (&other != this)
		{
			Name = other.Name;
			TypeName = other.TypeName;
			Type = other.Type;
			IsReadOnly = other.IsReadOnly;
			m_MonoClassField = other.m_MonoClassField;
			m_MonoProperty = other.m_MonoProperty;

			if (Type != FieldType::String)
			{
				m_StoredValueBuffer = AllocateBuffer(Type);
				memcpy(m_StoredValueBuffer, other.m_StoredValueBuffer, GetFieldSize(Type));
			}
			else
			{
				m_StoredValueBuffer = other.m_StoredValueBuffer;
			}
		}

		return *this;
	}

	void PublicField::CopyStoredValueFromRuntime(EntityInstance& entityInstance)
	{
		MonoObject* monoInstance = entityInstance.GetMonoInstance();
		EG_CORE_ASSERT(monoInstance, "No mono instance");

		if (Type == FieldType::String)
		{
			if (m_MonoProperty)
			{
				MonoString* str = (MonoString*)mono_property_get_value(m_MonoProperty, monoInstance, nullptr, nullptr);
				auto& stringValue = s_PublicFieldStringValues[Name];
				stringValue = mono_string_to_utf8(str);
				m_StoredValueBuffer = (uint8_t*)(&stringValue);
			}
			else
			{
				MonoString* str;
				mono_field_get_value(monoInstance, m_MonoClassField, &str);
				auto& stringValue = s_PublicFieldStringValues[Name];
				stringValue = mono_string_to_utf8(str);
				m_StoredValueBuffer = (uint8_t*)(&stringValue);
			}
		}
		else
		{
			if (m_MonoProperty)
			{
				MonoObject* result = mono_property_get_value(m_MonoProperty, monoInstance, nullptr, nullptr);
				memcpy(m_StoredValueBuffer, mono_object_unbox(result), GetFieldSize(Type));
			}
			else
			{
				mono_field_get_value(monoInstance, m_MonoClassField, m_StoredValueBuffer);
			}
		}
	}

}
