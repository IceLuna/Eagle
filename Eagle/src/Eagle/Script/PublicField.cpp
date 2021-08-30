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

	PublicField& PublicField::operator=(PublicField&& other) noexcept
	{
		Name = std::move(other.Name);
		TypeName = std::move(other.TypeName);
		Type = std::move(other.Type);
		IsReadOnly = std::move(other.IsReadOnly);
		m_MonoClassField = std::move(other.m_MonoClassField);
		m_MonoProperty = std::move(other.m_MonoProperty);

		m_StoredValueBuffer = other.m_StoredValueBuffer;
		other.m_StoredValueBuffer = nullptr;

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

	void PublicField::CopyStoredValueToRuntime(EntityInstance& entityInstance)
	{
		MonoObject* monoInstance = entityInstance.GetMonoInstance();
		EG_CORE_ASSERT(monoInstance, "No mono instance");

		if (IsReadOnly)
			return;

		if (Type == FieldType::ClassReference)
		{
			EG_CORE_ASSERT(!TypeName.empty(), "Empty TypeName");

			void* params[] = { &m_StoredValueBuffer };
			MonoObject* obj = ScriptEngine::Construct(TypeName + ":.ctor(intptr)", true, params);
			mono_field_set_value(monoInstance, m_MonoClassField, obj);
		}
		else if (Type == FieldType::String)
		{
			SetRuntimeValue_Internal(entityInstance, *((std::string*)m_StoredValueBuffer));
		}
		else
		{
			SetRuntimeValue_Internal(entityInstance, m_StoredValueBuffer);
		}
	}

	void PublicField::SetRuntimeValue_Internal(EntityInstance& entityInstance, void* value)
	{
		MonoObject* monoInstance = entityInstance.GetMonoInstance();
		EG_CORE_ASSERT(monoInstance, "No mono instance");

		if (IsReadOnly)
			return;

		if (m_MonoProperty)
		{
			void* data[] = { value };
			mono_property_set_value(m_MonoProperty, monoInstance, data, nullptr);
		}
		else
		{
			mono_field_set_value(monoInstance, m_MonoClassField, value);
		}
	}

	void PublicField::SetRuntimeValue_Internal(EntityInstance& entityInstance, const std::string& value)
	{
		MonoObject* monoInstance = entityInstance.GetMonoInstance();
		EG_CORE_ASSERT(monoInstance, "No mono instance");

		if (IsReadOnly)
			return;

		MonoString* monoString = mono_string_new(mono_domain_get(), value.c_str());

		if (m_MonoProperty)
		{
			void* data[] = { monoString };
			mono_property_set_value(m_MonoProperty, monoInstance, data, nullptr);
		}
		else
		{
			mono_field_set_value(monoInstance, m_MonoClassField, monoString);
		}
	}

	void PublicField::GetRuntimeValue_Internal(EntityInstance& entityInstance, void* outValue) const
	{
		MonoObject* monoInstance = entityInstance.GetMonoInstance();
		EG_CORE_ASSERT(monoInstance, "No mono instance");

		if (m_MonoProperty)
		{
			MonoObject* result = mono_property_get_value(m_MonoProperty, monoInstance, nullptr, nullptr);
			memcpy(outValue, result, GetFieldSize(Type));
		}
		else
		{
			mono_field_get_value(monoInstance, m_MonoClassField, outValue);
		}
	}

	void PublicField::GetRuntimeValue_Internal(EntityInstance& entityInstance, std::string& outValue) const
	{
		MonoObject* monoInstance = entityInstance.GetMonoInstance();
		EG_CORE_ASSERT(monoInstance, "No mono instance");

		MonoString* monoString = nullptr;

		if (m_MonoProperty)
			monoString = (MonoString*)mono_property_get_value(m_MonoProperty, monoInstance, nullptr, nullptr);
		else
			mono_field_get_value(monoInstance, m_MonoClassField, monoString);

		outValue = mono_string_to_utf8(monoString);
	}

}
