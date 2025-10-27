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

		size_t GetRuntimeArrayLength(MonoObject* instance) const;

		void CopyStoredValueFromRuntime(MonoObject* instance);
		void CopyStoredValueToRuntime(MonoObject* instance) const;

		// Returns false if failed
		bool CopyStoredValue(const PublicField& other);
		bool IsStoredValueEqual(const PublicField& other);

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
				T value;
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

		void AllocateBuffer(FieldType type)
		{
			m_StoredValueBuffer.Allocate(m_FieldSize * ArrayLength);
			if (type == FieldType::String)
			{
				std::string* basePtr = (std::string*)m_StoredValueBuffer.Data();
				for (size_t i = 0; i < ArrayLength; ++i)
				{
					new (basePtr) std::string();
					basePtr++;
				}
			}
			else
			{
				memset(m_StoredValueBuffer.Data(), 0, m_FieldSize);
			}
		}

		// @idx. Used if it's an array
		std::string& GetDataAsString(size_t idx = 0)
		{
			std::string* base = (std::string*)(m_StoredValueBuffer.Data());
			return *(base + idx);
		}

		// @idx. Used if it's an array
		const std::string& GetDataAsString(size_t idx = 0) const
		{
			const std::string* base = (const std::string*)(m_StoredValueBuffer.Data());
			return *(base + idx);
		}

	private:
		MonoClassField* m_MonoClassField = nullptr;
		MonoProperty* m_MonoProperty = nullptr;
		ScopedDataBuffer m_StoredValueBuffer;
		uint32_t m_FieldSize = 0u;

		friend class ScriptEngine;
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
