#include "egpch.h"
#include "PublicField.h"
#include "ScriptEngine.h"
#include "Eagle/Core/GUID.h"

#include <mono/jit/jit.h>

namespace Eagle
{
	// If fails, please make sure `m_StoredValueBuffer` uses correct alignment when allocating `std::string`
	static_assert(alignof(std::string) <= alignof(std::max_align_t));

	static uint32_t GetFieldSize(FieldType type)
	{
		switch (type)
		{
			case FieldType::Int: return 4;
			case FieldType::UnsignedInt: return 4;
			case FieldType::Float: return 4;
			case FieldType::String: return sizeof(std::string);
			case FieldType::Vec2: return 4 * 2;
			case FieldType::Vec3: return 4 * 3;
			case FieldType::Vec4: return 4 * 4;
			case FieldType::Bool: return 1;
			case FieldType::Color3: return 4 * 3;
			case FieldType::Color4: return 4 * 4;
			case FieldType::Enum: return 4;
			case FieldType::Entity:
			case FieldType::Asset:
			case FieldType::AssetTexture2D:
			case FieldType::AssetTextureCube:
			case FieldType::AssetStaticMesh:
			case FieldType::AssetSkeletalMesh:
			case FieldType::AssetAudio:
			case FieldType::AssetSoundGroup:
			case FieldType::AssetFont:
			case FieldType::AssetMaterial:
			case FieldType::AssetPhysicsMaterial:
			case FieldType::AssetEntity:
			case FieldType::AssetScene:
			case FieldType::AssetAnimation:
			case FieldType::AssetAnimationGraph:
			case FieldType::AssetParticleSystem:
			case FieldType::AssetAnimationBlendSpace:
			case FieldType::AssetBehaviorGraph:
				return sizeof(GUID);
		}
		EG_CORE_ASSERT(false, "Unknown type size");
		return 0;
	}

	// @bytes how many bytes to get(?) (mono API requires it). Must be retrieved via `PublicField::GetFieldSize()`
	static void* GetArrayData(MonoArray* array, int bytes, size_t index)
	{
		return mono_array_addr_with_size(array, bytes, index);
	}

	PublicField::PublicField(const std::string& fullName, const std::string& name, const std::string& typeName, const std::string& tooltip, FieldType type, bool bArray, size_t arrayLength)
		: FullName(fullName), UIName(name), TypeName(typeName), Tooltip(tooltip), Type(type)
		, bArray(bArray), ArrayLength(arrayLength), m_FieldSize(GetFieldSize(Type))
	{
		AllocateBuffer(Type);
	}

	PublicField::PublicField(std::string&& fullName, std::string&& name, std::string&& typeName, std::string&& tooltip, FieldType type, bool bArray, size_t arrayLength)
		: FullName(std::move(fullName)), UIName(std::move(name)), TypeName(std::move(typeName)), Tooltip(std::move(tooltip)), Type(type)
		, bArray(bArray), ArrayLength(arrayLength), m_FieldSize(GetFieldSize(Type))
	{
		AllocateBuffer(Type);
	}

	PublicField::PublicField(const PublicField& other)
		: FullName(other.FullName), UIName(other.UIName), TypeName(other.TypeName)
		, Tooltip(other.Tooltip), Type(other.Type), bArray(other.bArray), ArrayLength(other.ArrayLength)
		, EnumFields(other.EnumFields)
		, m_MonoClassField(other.m_MonoClassField)
		, m_MonoProperty(other.m_MonoProperty)
		, m_FieldSize(other.m_FieldSize)
	{
		AllocateBuffer(Type);
		CopyStoredValue(other);
	}

	PublicField::~PublicField()
	{
		if (Type == FieldType::String && m_StoredValueBuffer.Size() > 0)
		{
			for (size_t i = 0; i < ArrayLength; ++i)
				GetDataAsString(i).~basic_string();
		}
	}

	PublicField& PublicField::operator=(const PublicField& other)
	{
		if (&other != this)
		{
			FullName = other.FullName;
			UIName = other.UIName;
			TypeName = other.TypeName;
			Tooltip = other.Tooltip;
			Type = other.Type;
			bArray = other.bArray;
			ArrayLength = other.ArrayLength;
			m_MonoClassField = other.m_MonoClassField;
			m_MonoProperty = other.m_MonoProperty;
			EnumFields = other.EnumFields;
			m_FieldSize = other.m_FieldSize;

			AllocateBuffer(Type);
			CopyStoredValue(other);
		}

		return *this;
	}

	size_t PublicField::GetRuntimeArrayLength(MonoObject* instance) const
	{
		if (!bArray)
			return 1; // Should be 1 to indicate there's a single element

		if (m_MonoClassField)
		{
			return ScriptEngine::GetMonoArrayLength(instance, m_MonoClassField);
		}
		else if (m_MonoProperty)
		{
			return ScriptEngine::GetMonoArrayLength(instance, m_MonoProperty);
		}

		return 0;
	}

	void PublicField::CopyStoredValueFromRuntime(MonoObject* instance)
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		if (bArray)
		{
			if (ArrayLength == 0)
				return;

			MonoArray* array = nullptr;
			if (m_MonoProperty)
			{
				array = (MonoArray*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
			}
			else if (m_MonoClassField)
			{
				mono_field_get_value(instance, m_MonoClassField, &array);
			}

			if (!array)
				return;

			if (Type == FieldType::String)
			{
				for (size_t i = 0; i < ArrayLength; ++i)
				{
					MonoString* str = mono_array_get(array, MonoString*, i);
					GetDataAsString(i) = MonoStringHandler(str).c_str();
				}
			}
			else
			{
				const int size = (int)m_FieldSize;
				for (size_t i = 0; i < ArrayLength; ++i)
				{
					const size_t offset = i * size_t(size);
					void* data = GetArrayData(array, size, i);
					if (data)
						m_StoredValueBuffer.Write(data, size, offset);
					else
						m_StoredValueBuffer.WriteZero(size, offset);
				}
			}

			return;
		}
		else
		{
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
					if (result)
						m_StoredValueBuffer.Write(mono_object_unbox(result), m_StoredValueBuffer.Size());
					else
						m_StoredValueBuffer.SetToZero();
				}
				else
				{
					mono_field_get_value(instance, m_MonoClassField, m_StoredValueBuffer.Data());
				}
			}
		}

		if (Type == FieldType::Enum && !EnumFields.empty())
		{
			// Set valid value
			for (size_t i = 0; i < ArrayLength; ++i)
			{
				const int enumValue = GetStoredValue<int>(0);
				if (EnumFields.find(enumValue) == EnumFields.end())
					SetStoredValue<int>(EnumFields.begin()->first, 0);
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

		if (Type == FieldType::String)
		{
			for (size_t i = 0; i < ArrayLength; ++i)
				SetRuntimeValue_Internal(instance, GetDataAsString(i), i);
		}
		else
		{
			uint8_t* base = (uint8_t*)m_StoredValueBuffer.Data();
			for (size_t i = 0; i < ArrayLength; ++i)
			{
				SetRuntimeValue_Internal(instance, base + i * m_FieldSize, i);
			}
		}
	}

	bool PublicField::CopyStoredValue(const PublicField& other)
	{
		if (Type != other.Type)
			return false;

		const size_t arrayLength = std::min(ArrayLength, other.ArrayLength);
		if (Type == FieldType::String)
		{
			for (size_t i = 0; i < arrayLength; ++i)
				GetDataAsString(i) = other.GetDataAsString(i);
		}
		else
		{
			m_StoredValueBuffer.Write(other.m_StoredValueBuffer.Data(), arrayLength * m_FieldSize);
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
			for (size_t i = 0; i < ArrayLength; ++i)
			{
				if (GetDataAsString(i) != other.GetDataAsString(i))
					return false;
			}
			return true;
		}
		else
		{
			return memcmp(m_StoredValueBuffer.Data(), other.m_StoredValueBuffer.Data(), m_StoredValueBuffer.Size()) == 0;
		}
	}

	void PublicField::SetRuntimeValue_Internal(MonoObject* instance, void* value, size_t idx) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		const size_t arrayLength = GetRuntimeArrayLength(instance);
		MonoArray* array = nullptr;
		if (bArray)
		{
			if (idx >= arrayLength)
			{
				EG_CORE_ERROR("Failed to write to an array {} ({}). Index ({}) is out of bounds ({})", UIName, FullName, idx, arrayLength);
				return;
			}

			if (m_MonoProperty)
			{
				array = (MonoArray*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
			}
			else if (m_MonoClassField)
			{
				mono_field_get_value(instance, m_MonoClassField, &array);
			}

			if (!array)
				return;
		}

		if (Type == FieldType::Entity || IsAssetType(Type))
		{
			GUID guid;
			memcpy(&guid, value, m_FieldSize);

			void* params[] = { value };
			MonoObject* obj = guid.IsNull() ? nullptr : ScriptEngine::Construct(TypeName + ":.ctor(Eagle.GUID)", true, params);

			if (bArray)
			{
				mono_array_set(array, MonoObject*, idx, obj);
			}
			else
			{
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
		}
		else
		{
			if (bArray)
			{
				void* dst = GetArrayData(array, m_FieldSize, idx);
				memcpy(dst, value, m_FieldSize);
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
	}

	void PublicField::SetRuntimeValue_Internal(MonoObject* instance, const std::string& value, size_t idx) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		const size_t arrayLength = GetRuntimeArrayLength(instance);
		MonoArray* array = nullptr;
		if (bArray)
		{
			if (idx >= arrayLength)
			{
				EG_CORE_ERROR("Failed to write to an array {} ({}). Index ({}) is out of bounds ({})", UIName, FullName, idx, arrayLength);
				return;
			}

			if (m_MonoProperty)
			{
				array = (MonoArray*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
			}
			else if (m_MonoClassField)
			{
				mono_field_get_value(instance, m_MonoClassField, &array);
			}

			if (!array)
				return;
		}

		MonoString* monoString = mono_string_new(mono_domain_get(), value.c_str());

		if (bArray)
		{
			mono_array_set(array, MonoString*, idx, monoString);
		}
		else
		{
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
	}

	void PublicField::GetRuntimeValue_Internal(MonoObject* instance, void* outValue, size_t idx) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		const size_t arrayLength = GetRuntimeArrayLength(instance);
		MonoArray* array = nullptr;
		if (bArray)
		{
			if (idx >= arrayLength)
			{
				EG_CORE_ERROR("Failed to write to an array {} ({}). Index ({}) is out of bounds ({})", UIName, FullName, idx, arrayLength);
				return;
			}

			if (m_MonoProperty)
			{
				array = (MonoArray*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
			}
			else if (m_MonoClassField)
			{
				mono_field_get_value(instance, m_MonoClassField, &array);
			}

			if (!array)
				return;
		}

		if (Type == FieldType::Entity || IsAssetType(Type))
		{
			MonoObject* obj;
			if (bArray)
			{
				obj = mono_array_get(array, MonoObject*, idx);
			}
			else
			{
				if (m_MonoProperty)
					obj = mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
				else
					mono_field_get_value(instance, m_MonoClassField, &obj);
			}

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
				memset(outValue, 0, m_FieldSize);
			}
		}
		else
		{
			if (bArray)
			{
				memcpy(outValue, GetArrayData(array, int(m_FieldSize), idx), m_FieldSize);
			}
			else
			{
				if (m_MonoProperty)
				{
					MonoObject* result = mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
					if (result)
						memcpy(outValue, mono_object_unbox(result), m_FieldSize);
					else
						memset(outValue, 0, m_FieldSize);
				}
				else
				{
					mono_field_get_value(instance, m_MonoClassField, outValue);
				}
			}
		}
	}

	void PublicField::GetRuntimeValue_Internal(MonoObject* instance, std::string& outValue, size_t idx) const
	{
		if (!instance)
		{
			EG_CORE_ASSERT(instance, "No mono instance");
			return;
		}

		const size_t arrayLength = GetRuntimeArrayLength(instance);
		MonoArray* array = nullptr;
		if (bArray)
		{
			if (idx >= arrayLength)
			{
				EG_CORE_ERROR("Failed to write to an array {} ({}). Index ({}) is out of bounds ({})", UIName, FullName, idx, arrayLength);
				return;
			}

			if (m_MonoProperty)
			{
				array = (MonoArray*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
			}
			else if (m_MonoClassField)
			{
				mono_field_get_value(instance, m_MonoClassField, &array);
			}

			if (!array)
				return;
		}

		MonoString* monoString = nullptr;

		if (bArray)
		{
			monoString = mono_array_get(array, MonoString*, idx);
		}
		else
		{
			if (m_MonoProperty)
				monoString = (MonoString*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
			else
				mono_field_get_value(instance, m_MonoClassField, &monoString);
		}

		outValue = MonoStringHandler(monoString).c_str();
	}

}
