#pragma once

#include "Eagle/Core/DataBuffer.h"

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
	typedef struct _MonoArray MonoArray;
}

namespace Eagle
{
	struct ScriptEnumData
	{
		std::string Name;
		std::string Tooltip;
	};
	// `Enum value` -> its data
	using ScriptEnumFields = std::map<int, ScriptEnumData>;

	//Add new type to Scene Serializer
	enum class FieldType : uint32_t
	{
		None, Int, UnsignedInt, Float, String, Vec2, Vec3, Vec4,
		Bool, Color3, Color4, Enum, Entity,
		Asset, AssetTexture2D, AssetTextureCube, AssetStaticMesh, AssetSkeletalMesh, AssetAudio, AssetSoundGroup,
		AssetFont, AssetMaterial, AssetPhysicsMaterial, AssetEntity, AssetScene, AssetAnimation, AssetAnimationGraph,
		AssetParticleSystem, AssetAnimationBlendSpace, AssetBehaviorGraph,
		Struct, // User-defined C# struct
	};

	inline bool IsAssetType(FieldType type)
	{
		switch (type)
		{
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
				return true;
			default:
				return false;
		}
	}

	class PublicField
	{
	public:
		std::string FullName;
		std::string UIName;
		std::string TypeName;
		std::string Tooltip;
		FieldType Type = FieldType::None;
		bool bArray = false;

		// Must be `1` for non-arrays.
		// Note: in runtime array length can change and this variable doesn't get update since it represents default field (not in runtime).
		// In order to get the runtime length, use `GetRuntimeArrayLength()`
		size_t ArrayLength = 1;
		
		// If `Type` is `Enum` then this can be used to fetch valid `names - values`
		ScriptEnumFields EnumFields;

		// If `Type` is `Struct`, this describes the struct members (with default values).
		// It acts as a template: every element of the field (`ArrayLength` of them) has its own copy of these members,
		// which can be accessed via `GetStructMembers(idx)`.
		// In runtime these can be used directly together with a boxed struct (see `EditRuntimeStruct()`).
		// Note: members are not sorted and only public (non-static) fields of a struct are supported, properties are not
		std::vector<PublicField> StructMembers;

		PublicField() = default;
		PublicField(const std::string& fullName, const std::string& uiName, const std::string& typeName, const std::string& toolTip, FieldType type,
			bool bArray, size_t arrayLength);
		PublicField(std::string&& fullName, std::string&& uiName, std::string&& typeName, std::string&& toolTip, FieldType type,
			bool bArray, size_t arrayLength);
		PublicField(const PublicField& other);
		PublicField(PublicField&& other) noexcept = default;
		~PublicField();

		PublicField& operator= (const PublicField& other);
		PublicField& operator= (PublicField&& other) noexcept = default;

		bool operator< (const PublicField& other) const { return UIName < other.UIName; }

		void SetMonoClassField(MonoClassField* value);
		void SetMonoProperty(MonoProperty* value);

		// Only valid if `Type` is `Struct`. Sets the struct members and resets stored values of all elements to the members' defaults
		void SetStructMembers(std::vector<PublicField>&& members);

		// Only valid if `Type` is `Struct`.
		// Returns stored members of an element.
		// @idx. Used if it's an array
		std::vector<PublicField>& GetStructMembers(size_t idx = 0);
		const std::vector<PublicField>& GetStructMembers(size_t idx = 0) const;

		// Only valid if `Type` is `Struct`.
		// Since structs are value types, their runtime value is a copy. This function:
		//   1) Retrieves a boxed copy of a struct (element `idx` if it's an array);
		//   2) Calls `func` with that boxed struct. It can be used as an `instance` for `StructMembers` (for example, `member.GetRuntimeValue<float>(boxedStruct)`);
		//   3) If `func` returns true, writes the (modified) boxed struct back to `instance`.
		// Returns what `func` returned (or false if it failed to retrieve the struct)
		bool EditRuntimeStruct(MonoObject* instance, size_t idx, const std::function<bool(MonoObject* boxedStruct)>& func) const;

		// Returns the index of the new element
		// @count. How many to append
		size_t AppendArrayElement(size_t count = 1);
		void RemoveArrayElement(size_t idx);
		void ClearArray();
		// Appends/removes elements at the end so that the array has `newLength` elements
		void ResizeArray(size_t newLength);

		// @count. How many to append
		size_t AppendRuntimeArrayElement(MonoObject* instance, size_t count = 1);
		void RemoveRuntimeArrayElement(MonoObject* instance, size_t idx);
		void ClearRuntimeArray(MonoObject* instance);

		size_t GetRuntimeArrayLength(MonoObject* instance) const;

		void CopyStoredValueFromRuntime(MonoObject* instance);
		void CopyStoredValueToRuntime(MonoObject* instance) const;

		// Returns false if failed
		bool CopyStoredValue(const PublicField& other);
		bool IsStoredValueEqual(const PublicField& other) const;

		// If `Type` is `Enum`, makes sure that stored values are valid enum values (if not, the first valid value is set).
		// If `Type` is `Struct`, it's applied to its members recursively.
		void ValidateEnumValues();

		// @idx. Used if it's an array
		template<typename T>
		T GetStoredValue(size_t idx = 0) const
		{
			if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string&> || std::is_same_v<T, const std::string&>)
			{
				return GetDataAsString(idx);
			}
			else
			{
				T value;
				GetStoredValue_Internal(&value, idx);
				return value;
			}
		}

		// @idx. Used if it's an array
		template <typename T>
		void SetStoredValue(const T& value, size_t idx = 0)
		{
			if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string&> || std::is_same_v<T, const std::string&>)
			{
				GetDataAsString(idx).assign(value);
			}
			else
			{
				SetStoredValue_Internal(&value, idx);
			}
		}

		// @idx. Used if it's an array
		template <typename T>
		T GetRuntimeValue(MonoObject* instance, size_t idx = 0) const
		{
			if constexpr (std::is_same_v<T, std::string>)
			{
				std::string value;
				GetRuntimeValue_Internal(instance, value, idx);
				return value;
			}
			else
			{
				T value = T{};
				GetRuntimeValue_Internal(instance, &value, idx);
				return value;
			}
		}

		// @idx. Used if it's an array
		template <typename T>
		void SetRuntimeValue(MonoObject* instance, const T& value, size_t idx = 0) const
		{
			if constexpr (std::is_same<std::string, T>::value)
			{
				SetRuntimeValue_Internal(instance, value, idx);
			}
			else
			{
				void* ptr = (void*)&value; // Removing const because for some reason `mono` accepts non-const-ptr
				SetRuntimeValue_Internal(instance, ptr, idx);
			}
		}

	private:
		// @idx. Used if it's an array
		void GetStoredValue_Internal(void* outValue, size_t idx = 0) const
		{
			const uint8_t* base = (uint8_t*)m_StoredValueBuffer.Data() + idx * m_FieldSize;
			memcpy(outValue, base, m_FieldSize);
		}

		// @idx. Used if it's an array
		void SetStoredValue_Internal(const void* value, size_t idx = 0)
		{
			m_StoredValueBuffer.Write(value, m_FieldSize, idx * m_FieldSize);
		}

		void SetRuntimeValue_Internal(MonoObject* instance, void* value, size_t idx = 0) const;
		void SetRuntimeValue_Internal(MonoObject* instance, const std::string& value, size_t idx = 0) const;
		void GetRuntimeValue_Internal(MonoObject* instance, void* outValue, size_t idx = 0) const;
		void GetRuntimeValue_Internal(MonoObject* instance, std::string& outValue, size_t idx = 0) const;

		void AllocateBuffer();
		void ReleaseBuffer();

		void SetRuntimeArray(MonoObject* instance, MonoArray* newArray) const;
		MonoArray* GetRuntimeArray(MonoObject* instance) const;

		// `Struct` helpers. Returns a boxed copy of a runtime struct
		MonoObject* GetRuntimeStructBoxed(MonoObject* instance, size_t idx) const;
		// Writes the value of `boxedStruct` into `instance`
		void SetRuntimeStructBoxed(MonoObject* instance, MonoObject* boxedStruct, size_t idx) const;

		// @idx. Used if it's an array
		std::string& GetDataAsString(size_t idx = 0);
		const std::string& GetDataAsString(size_t idx = 0) const;

	private:
		MonoClass* m_Class = nullptr;
		MonoClassField* m_MonoClassField = nullptr;
		MonoProperty* m_MonoProperty = nullptr;
		ScopedDataBuffer m_StoredValueBuffer;
		// If `Type` is `Struct`, stored values are kept here instead of `m_StoredValueBuffer`. One element per array element
		std::vector<std::vector<PublicField>> m_StructElements;
		uint32_t m_FieldSize = 0u;
	};
}

namespace std
{
	template <>
	struct hash<Eagle::PublicField>
	{
		std::size_t operator()(const Eagle::PublicField& field) const
		{
			return std::hash<std::string>()(field.UIName);
		}
	};
}
