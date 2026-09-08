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
		return 1;
	}

	static std::string& GetDataAsString_Internal(ScopedDataBuffer& buffer, size_t idx)
	{
		std::string* base = (std::string*)(buffer.Data());
		return *(base + idx);
	}

	static const std::string& GetDataAsString_Internal(const ScopedDataBuffer& buffer, size_t idx)
	{
		const std::string* base = (const std::string*)(buffer.Data());
		return *(base + idx);
	}

	static void ReleaseBuffer_Internal(ScopedDataBuffer* buffer, FieldType type, size_t fieldSize)
	{
		const size_t length = buffer->Size() / fieldSize;
		if (type == FieldType::String && buffer->Size() > 0)
		{
			for (size_t i = 0; i < length; ++i)
				GetDataAsString_Internal(*buffer, i).~basic_string();
		}
		buffer->Release();
	}

	static void AllocateBuffer_Internal(FieldType type, ScopedDataBuffer* buffer, size_t fieldSize, size_t length)
	{
		ReleaseBuffer_Internal(buffer, type, fieldSize);
		if (length == 0)
			return;

		buffer->Allocate(fieldSize * length);
		if (type == FieldType::String)
		{
			std::string* basePtr = (std::string*)buffer->Data();
			for (size_t i = 0; i < length; ++i)
			{
				new (basePtr) std::string();
				basePtr++;
			}
		}
		else
		{
			buffer->WriteZero(buffer->Size());
		}
	}

	static void CopyArrayValuesAfterPush(MonoArray* oldArray, MonoArray* newArray, size_t oldLength)
	{
		for (size_t i = 0; i < oldLength; ++i)
		{
			MonoObject* obj = mono_array_get(oldArray, MonoObject*, i);
			mono_array_setref(newArray, i, obj);
		}
	};

	static void CopyArrayValuesAfterPop(MonoArray* oldArray, MonoArray* newArray, size_t removedIndex, size_t oldLength)
	{
		for (size_t i = 0; i < removedIndex; ++i)
		{
			MonoObject* obj = mono_array_get(oldArray, MonoObject*, i);
			mono_array_setref(newArray, i, obj);
		}

		for (size_t i = removedIndex + 1; i < oldLength; ++i)
		{
			MonoObject* obj = mono_array_get(oldArray, MonoObject*, i);
			mono_array_setref(newArray, i - 1, obj);
		}
	};

	// @bytes how many bytes to get(?) (mono API requires it). Must be retrieved via `PublicField::GetFieldSize()`
	static void* GetArrayData(MonoArray* array, int bytes, size_t index)
	{
		return mono_array_addr_with_size(array, bytes, index);
	}

	PublicField::PublicField(const std::string& fullName, const std::string& name, const std::string& typeName, const std::string& tooltip, FieldType type, bool bArray, size_t arrayLength)
		: FullName(fullName), UIName(name), TypeName(typeName), Tooltip(tooltip), Type(type)
		, bArray(bArray), ArrayLength(arrayLength), m_FieldSize(GetFieldSize(Type))
	{
		AllocateBuffer();
	}

	PublicField::PublicField(std::string&& fullName, std::string&& name, std::string&& typeName, std::string&& tooltip, FieldType type, bool bArray, size_t arrayLength)
		: FullName(std::move(fullName)), UIName(std::move(name)), TypeName(std::move(typeName)), Tooltip(std::move(tooltip)), Type(type)
		, bArray(bArray), ArrayLength(arrayLength), m_FieldSize(GetFieldSize(Type))
	{
		AllocateBuffer();
	}

	PublicField::PublicField(const PublicField& other)
		: FullName(other.FullName), UIName(other.UIName), TypeName(other.TypeName)
		, Tooltip(other.Tooltip), Type(other.Type), bArray(other.bArray), ArrayLength(other.ArrayLength)
		, EnumFields(other.EnumFields)
		, m_Class(other.m_Class)
		, m_MonoClassField(other.m_MonoClassField)
		, m_MonoProperty(other.m_MonoProperty)
		, m_FieldSize(other.m_FieldSize)
	{
		AllocateBuffer();
		CopyStoredValue(other);
	}

	PublicField::~PublicField()
	{
		ReleaseBuffer();
	}

	PublicField& PublicField::operator=(const PublicField& other)
	{
		if (&other != this)
		{
			ReleaseBuffer();

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
			m_Class = other.m_Class;

			AllocateBuffer();
			CopyStoredValue(other);
		}

		return *this;
	}

	void PublicField::SetMonoClassField(MonoClassField* value)
	{
		m_MonoClassField = value;
		m_MonoProperty = nullptr;
		m_Class = nullptr;

		if (m_MonoClassField)
		{
			if (MonoType* fieldType = mono_field_get_type(m_MonoClassField))
				if (MonoClass* returnClass = mono_class_from_mono_type(fieldType))
					m_Class = bArray ? mono_class_get_element_class(returnClass) : returnClass;
		}

		EG_CORE_ASSERT(m_Class);
		if (!m_Class)
		{
			EG_CORE_ERROR("Failed to retrieve field class for: {}", FullName);
		}
	}

	void PublicField::SetMonoProperty(MonoProperty* value)
	{
		m_MonoProperty = value;
		m_MonoClassField = nullptr;
		m_Class = nullptr;

		if (m_MonoProperty)
		{
			if (MonoMethod* getter = mono_property_get_get_method(m_MonoProperty))
				if (MonoMethodSignature* signature = mono_method_signature(getter))
					if (MonoType* returnType = mono_signature_get_return_type(signature))
						if (MonoClass* returnClass = mono_class_from_mono_type(returnType))
							m_Class = bArray ? mono_class_get_element_class(returnClass) : returnClass;
		}

		EG_CORE_ASSERT(m_Class);
		if (!m_Class)
		{
			EG_CORE_ERROR("Failed to retrieve property class for: {}", FullName);
		}
	}

	size_t PublicField::AppendArrayElement()
	{
		if (!bArray)
			return 0;

		const size_t oldLength = ArrayLength;
		ArrayLength++;

		ScopedDataBuffer newArray;
		AllocateBuffer_Internal(Type, &newArray, m_FieldSize, ArrayLength);

		// Copy existing values
		if (oldLength > 0)
		{
			if (Type == FieldType::String)
			{
				for (size_t i = 0; i < oldLength; ++i)
					GetDataAsString_Internal(newArray, i) = std::move(GetDataAsString(i));
			}
			else
			{
				newArray.Write(m_StoredValueBuffer.Data(), oldLength * m_FieldSize);
			}
		}

		ReleaseBuffer();
		m_StoredValueBuffer = std::move(newArray);
		return oldLength;
	}

	void PublicField::RemoveArrayElement(size_t idx)
	{
		if (!bArray)
			return;

		if (idx >= ArrayLength)
			return; // Invalid index

		const size_t oldLength = ArrayLength;
		ArrayLength--;

		ScopedDataBuffer newArray;
		AllocateBuffer_Internal(Type, &newArray, m_FieldSize, ArrayLength);

		// Copy existing values
		if (ArrayLength > 0)
		{
			if (Type == FieldType::String)
			{
				for (size_t i = 0; i < idx; ++i)
					GetDataAsString_Internal(newArray, i) = std::move(GetDataAsString(i));

				for (size_t i = idx + 1; i < oldLength; ++i)
					GetDataAsString_Internal(newArray, i - 1) = std::move(GetDataAsString(i));
			}
			else
			{
				const uint8_t* origData = (const uint8_t*)m_StoredValueBuffer.Data();

				size_t offset = 0;
				newArray.Write(origData, idx * m_FieldSize, offset);
				offset += idx * m_FieldSize;

				const size_t origDataOffset = (idx + 1) * m_FieldSize;
				newArray.Write(origData + origDataOffset, (oldLength - idx - 1) * m_FieldSize, offset);
			}
		}

		ReleaseBuffer();
		m_StoredValueBuffer = std::move(newArray);
	}

	size_t PublicField::AppendRuntimeArrayElement(MonoObject* instance)
	{
		if (!bArray || !m_Class)
			return 0;

		const size_t oldLength = GetRuntimeArrayLength(instance);
		const size_t newLength = oldLength + 1;

		MonoArray* oldArray = nullptr;
		if (m_MonoProperty)
		{
			oldArray = (MonoArray*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
		}
		else if (m_MonoClassField)
		{
			mono_field_get_value(instance, m_MonoClassField, &oldArray);
		}

		// Root the old array because mono_array_new() can trigger GC.
		uint32_t oldArrayHandle = 0;
		if (oldArray)
		{
			oldArrayHandle = mono_gchandle_new((MonoObject*)oldArray, true);
		}

		MonoArray* newArray = mono_array_new(mono_domain_get(), m_Class, newLength);
		const uint32_t newArrayHandle = mono_gchandle_new((MonoObject*)newArray, true);

		if (oldArray)
		{
			// Copy existing values
			if (Type == FieldType::String || Type == FieldType::Entity || IsAssetType(Type))
			{
				CopyArrayValuesAfterPush(oldArray, newArray, oldLength);
			}
			else
			{
				for (size_t i = 0; i < oldLength; ++i)
				{
					void* src = GetArrayData(oldArray, m_FieldSize, i);
					void* dst = GetArrayData(newArray, m_FieldSize, i);
					memcpy(dst, src, m_FieldSize);
				}
			}

			mono_gchandle_free(oldArrayHandle);
		}

		SetRuntimeArray(instance, newArray);
		mono_gchandle_free(newArrayHandle);

		return oldLength;
	}

	void PublicField::RemoveRuntimeArrayElement(MonoObject* instance, size_t idx)
	{
		if (!bArray || !m_Class)
			return;

		const size_t length = GetRuntimeArrayLength(instance);
		if (idx >= length)
			return; // Invalid index

		const size_t oldLength = length;
		const size_t newLength = oldLength - 1;

		MonoArray* oldArray = nullptr;
		if (m_MonoProperty)
		{
			oldArray = (MonoArray*)mono_property_get_value(m_MonoProperty, instance, nullptr, nullptr);
		}
		else if (m_MonoClassField)
		{
			mono_field_get_value(instance, m_MonoClassField, &oldArray);
		}

		// Root the old array because mono_array_new() can trigger GC.
		uint32_t oldArrayHandle = 0;
		if (oldArray)
		{
			oldArrayHandle = mono_gchandle_new((MonoObject*)oldArray, true);
		}

		MonoArray* newArray = mono_array_new(mono_domain_get(), m_Class, newLength);
		const uint32_t newArrayHandle = mono_gchandle_new((MonoObject*)newArray, true);
		if (oldArray)
		{
			// Copy existing values
			if (Type == FieldType::String || Type == FieldType::Entity || IsAssetType(Type))
			{
				CopyArrayValuesAfterPop(oldArray, newArray, idx, oldLength);
			}
			else
			{
				for (size_t i = 0; i < idx; ++i)
				{
					void* src = GetArrayData(oldArray, m_FieldSize, i);
					void* dst = GetArrayData(newArray, m_FieldSize, i);
					memcpy(dst, src, m_FieldSize);
				}

				for (size_t i = idx + 1; i < oldLength; ++i)
				{
					void* src = GetArrayData(oldArray, m_FieldSize, i);
					void* dst = GetArrayData(newArray, m_FieldSize, i - 1);
					memcpy(dst, src, m_FieldSize);
				}
			}
			mono_gchandle_free(oldArrayHandle);
		}

		SetRuntimeArray(instance, newArray);
		mono_gchandle_free(newArrayHandle);
	}

	void PublicField::ClearRuntimeArray(MonoObject* instance)
	{
		if (!bArray || !m_Class)
			return;

		SetRuntimeArray(instance, mono_array_new(mono_domain_get(), m_Class, 0));
	}

	void PublicField::ClearArray()
	{
		if (!bArray)
			return;

		ReleaseBuffer();
		ArrayLength = 0;
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

			ArrayLength = mono_array_length(array);
			AllocateBuffer();

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
				const int enumValue = GetStoredValue<int>(i);
				if (EnumFields.find(enumValue) == EnumFields.end())
					SetStoredValue<int>(EnumFields.begin()->first, i);
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

		// Allocate enought space for the runtime array
		if (bArray)
		{
			if (!m_Class)
			{
				EG_CORE_ERROR("Failed to retrieve array element class: {}", FullName);
				return;
			}

			MonoArray* array = mono_array_new(mono_domain_get(), m_Class, ArrayLength);
			SetRuntimeArray(instance, array);
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
		if (Type != other.Type || bArray != other.bArray)
			return false;

		if (bArray)
		{
			ArrayLength = other.ArrayLength;
			AllocateBuffer();
		}

		if (ArrayLength == 0)
			return false;

		if (Type == FieldType::String)
		{
			for (size_t i = 0; i < ArrayLength; ++i)
				GetDataAsString(i) = other.GetDataAsString(i);
		}
		else
		{
			m_StoredValueBuffer.Write(other.m_StoredValueBuffer.Data(), ArrayLength * m_FieldSize);
		}
		return true;
	}

	bool PublicField::IsStoredValueEqual(const PublicField& other)
	{
		if (Type != other.Type || ArrayLength != other.ArrayLength)
			return false;

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
			if (m_StoredValueBuffer.Size() != other.m_StoredValueBuffer.Size())
				return false;

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

		MonoArray* array = nullptr;
		if (bArray)
		{
			const size_t arrayLength = GetRuntimeArrayLength(instance);
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
				mono_array_setref(array, idx, obj);
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

		MonoArray* array = nullptr;
		if (bArray)
		{
			const size_t arrayLength = GetRuntimeArrayLength(instance);
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
			mono_array_setref(array, idx, monoString);
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
			EG_CORE_ERROR("Invalid C# instance");
			return;
		}

		MonoArray* array = nullptr;
		if (bArray)
		{
			const size_t arrayLength = GetRuntimeArrayLength(instance);
			if (idx >= arrayLength)
			{
				EG_CORE_ERROR("Failed to read array {} ({}). Index ({}) is out of bounds ({})", UIName, FullName, idx, arrayLength);
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
			MonoObject* obj = nullptr;
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
			EG_CORE_ERROR("Invalid C# instance");
			return;
		}

		MonoArray* array = nullptr;
		if (bArray)
		{
			const size_t arrayLength = GetRuntimeArrayLength(instance);
			if (idx >= arrayLength)
			{
				EG_CORE_ERROR("Failed to read array {} ({}). Index ({}) is out of bounds ({})", UIName, FullName, idx, arrayLength);
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
			{
				EG_CORE_ERROR("Failed to read array: {}", FullName);
				return;
			}
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

	void PublicField::AllocateBuffer()
	{
		AllocateBuffer_Internal(Type, &m_StoredValueBuffer, m_FieldSize, ArrayLength);
	}

	void PublicField::ReleaseBuffer()
	{
		ReleaseBuffer_Internal(&m_StoredValueBuffer, Type, m_FieldSize);
	}

	void PublicField::SetRuntimeArray(MonoObject* instance, MonoArray* newArray) const
	{
		if (m_MonoProperty)
		{
			void* params[] = { newArray };
			mono_property_set_value(m_MonoProperty, instance, params, nullptr);
		}
		else if (m_MonoClassField)
		{
			mono_field_set_value(instance, m_MonoClassField, newArray);
		}
		else
		{
			EG_CORE_ASSERT(false);
		}
	}
	
	std::string& PublicField::GetDataAsString(size_t idx)
	{
		return GetDataAsString_Internal(m_StoredValueBuffer, idx);
	}

	const std::string& PublicField::GetDataAsString(size_t idx) const
	{
		return GetDataAsString_Internal(m_StoredValueBuffer, idx);
	}
}
