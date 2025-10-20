#include "egpch.h"
#include "PublicField.h"
#include "ScriptEngine.h"
#include "Eagle/Core/GUID.h"

#include <mono/jit/jit.h>

namespace Eagle
{
	// If fails, please make sure `m_StoredValueBuffer` uses correct alignment when allocating `std::string`
	static_assert(alignof(std::string) <= alignof(std::max_align_t));

	PublicField::PublicField(const std::string& name, const std::string& typeName, const std::string& tooltip, FieldType type, bool isReadOnly)
	: UIName(name), TypeName(typeName), Tooltip(tooltip), Type(type), IsReadOnly(isReadOnly)
	{
		AllocateBuffer(Type);
	}

	PublicField::PublicField(std::string&& name, std::string&& typeName, std::string&& tooltip, FieldType type, bool isReadOnly)
		: UIName(std::move(name)), TypeName(std::move(typeName)), Tooltip(std::move(tooltip)), Type(type), IsReadOnly(isReadOnly)
	{
		AllocateBuffer(Type);
	}

	PublicField::PublicField(const PublicField& other)
		: UIName(other.UIName), TypeName(other.TypeName), Tooltip(other.Tooltip), Type(other.Type), IsReadOnly(other.IsReadOnly)
		, EnumFields(other.EnumFields)
		, m_MonoClassField(other.m_MonoClassField)
		, m_MonoProperty(other.m_MonoProperty)
	{
		AllocateBuffer(Type);
		CopyStoredValue(other);
	}

	PublicField::~PublicField()
	{
		if (Type == FieldType::String && m_StoredValueBuffer.Size() > 0)
		{
			GetDataAsString().~basic_string();
		}
	}

	PublicField& PublicField::operator=(const PublicField& other)
	{
		if (&other != this)
		{
			UIName = other.UIName;
			TypeName = other.TypeName;
			Tooltip = other.Tooltip;
			Type = other.Type;
			IsReadOnly = other.IsReadOnly;
			m_MonoClassField = other.m_MonoClassField;
			m_MonoProperty = other.m_MonoProperty;
			EnumFields = other.EnumFields;

			AllocateBuffer(Type);
			CopyStoredValue(other);
		}

		return *this;
	}

	void PublicField::CopyStoredValueFromRuntime(MonoObject* instance)
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		if (Type == FieldType::String)
		{
			if (m_MonoProperty)
			{
				MonoString* str = (MonoString*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
				GetDataAsString() = MonoStringHandler(str).c_str();
			}
			else
			{
				MonoString* str;
				mono_field_get_value(instance, m_MonoClassField, &str);
				auto& stringValue = GetDataAsString();
				if (str)
					stringValue = MonoStringHandler(str).c_str();
				else
					stringValue.clear();
			}
		}
		else
		{
			if (m_MonoProperty)
			{
				MonoObject* result = mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
				m_StoredValueBuffer.Write(mono_object_unbox(result), m_StoredValueBuffer.Size());
			}
			else
			{
				mono_field_get_value(instance, m_MonoClassField, m_StoredValueBuffer.Data());
			}
		}
	}

	void PublicField::CopyStoredValueToRuntime(MonoObject* instance) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		if (IsReadOnly)
			return;

		if (Type == FieldType::ClassReference)
		{
			EG_CORE_ASSERT(!TypeName.empty(), "Empty TypeName");

			void* params[] = { (void*)&m_StoredValueBuffer};
			MonoObject* obj = ScriptEngine::Construct(TypeName + ":.ctor(intptr)", true, params);
			mono_field_set_value(instance, m_MonoClassField, obj);
		}
		else if (Type == FieldType::String)
		{
			SetRuntimeValue_Internal(instance, GetDataAsString());
		}
		else if (Type == FieldType::Entity || IsAssetType(Type))
		{
			EG_CORE_ASSERT(!TypeName.empty(), "Empty TypeName");

			GUID guid = GetStoredValue<GUID>();
			if (guid.IsNull())
			{
				mono_field_set_value(instance, m_MonoClassField, nullptr);
			}
			else
			{
				void* params[] = { (void*)m_StoredValueBuffer.Data()};
				MonoObject* obj = ScriptEngine::Construct(TypeName + ":.ctor(Eagle.GUID)", true, params);
				mono_field_set_value(instance, m_MonoClassField, obj);
			}
		}
		else
		{
			SetRuntimeValue_Internal(instance, (void*)m_StoredValueBuffer.Data());
		}
	}

	bool PublicField::CopyStoredValue(const PublicField& other)
	{
		if (Type != other.Type)
			return false;

		EG_CORE_ASSERT(m_StoredValueBuffer.Size() == other.m_StoredValueBuffer.Size());
		if (Type == FieldType::String)
		{
			GetDataAsString() = other.GetDataAsString();
		}
		else
		{
			m_StoredValueBuffer.Write(other.m_StoredValueBuffer.Data(), other.m_StoredValueBuffer.Size());
		}
		return true;
	}

	bool PublicField::IsStoredValueEqual(const PublicField& other)
	{
		if (Type != other.Type)
			return false;

		EG_CORE_ASSERT(m_StoredValueBuffer.Size() == other.m_StoredValueBuffer.Size());
		if (Type == FieldType::String)
		{
			return GetDataAsString() == other.GetDataAsString();
		}
		else
		{
			return memcmp(m_StoredValueBuffer.Data(), other.m_StoredValueBuffer.Data(), m_StoredValueBuffer.Size()) == 0;
		}
	}

	void PublicField::SetRuntimeValue_Internal(MonoObject* instance, void* value) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		if (IsReadOnly)
			return;

		if (Type == FieldType::Entity || IsAssetType(Type))
		{
			GUID guid;
			memcpy(&guid, value, GetFieldSize(Type));

			void* params[] = { value };
			MonoObject* obj = nullptr;
			if (!guid.IsNull())
			{
				obj = ScriptEngine::Construct(TypeName + ":.ctor(Eagle.GUID)", true, params);
				mono_field_set_value(instance, m_MonoClassField, obj);
			}

			if (m_MonoProperty)
			{
				params[0] = { obj };
				mono_property_set_value(m_MonoProperty, instance, params, nullptr);
			}
			else
			{
				mono_field_set_value(instance, m_MonoClassField, obj);
			}
		}
		else
		{
			if (m_MonoProperty)
			{
				void* data[] = { value };
				mono_property_set_value(m_MonoProperty, instance, data, nullptr);
			}
			else
			{
				mono_field_set_value(instance, m_MonoClassField, value);
			}
		}
	}

	void PublicField::SetRuntimeValue_Internal(MonoObject* instance, const std::string& value) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		if (IsReadOnly)
			return;

		MonoString* monoString = mono_string_new(mono_domain_get(), value.c_str());

		if (m_MonoProperty)
		{
			void* data[] = { monoString };
			mono_property_set_value(m_MonoProperty, instance, data, nullptr);
		}
		else
		{
			mono_field_set_value(instance, m_MonoClassField, monoString);
		}
	}

	void PublicField::GetRuntimeValue_Internal(MonoObject* instance, void* outValue) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		if (Type == FieldType::Entity || IsAssetType(Type))
		{
			MonoObject* obj;
			if (m_MonoProperty)
				obj = mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
			else
				mono_field_get_value(instance, m_MonoClassField, &obj);

			if (obj)
			{
				const std::string test = Utils::GetEnumName(Type);
				if (Type == FieldType::Entity)
				{
					MonoClass* entityClass = ScriptEngine::GetEntityClass();
					MonoClassField* field = mono_class_get_field_from_name(entityClass, "<ID>k__BackingField"); // For some reason, `ID` field has this name in mono.
					mono_field_get_value(obj, field, outValue);
				}
				else
				{
					MonoClass* assetClass = ScriptEngine::GetAssetClass();
					MonoClassField* field = mono_class_get_field_from_name(assetClass, "m_GUID");
					mono_field_get_value(obj, field, outValue);
				}
			}
			else
			{
				*((GUID*)outValue) = GUID(0, 0);
			}
		}
		else
		{
			if (m_MonoProperty)
			{
				MonoObject* result = mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
				memcpy(outValue, result, GetFieldSize(Type));
			}
			else
			{
				mono_field_get_value(instance, m_MonoClassField, outValue);
			}
		}
	}

	void PublicField::GetRuntimeValue_Internal(MonoObject* instance, std::string& outValue) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		MonoString* monoString = nullptr;

		if (m_MonoProperty)
			monoString = (MonoString*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
		else
			mono_field_get_value(instance, m_MonoClassField, &monoString);

		outValue = MonoStringHandler(monoString).c_str();
	}

}
