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
		std::string UIName;
		std::string TypeName;
		std::string Tooltip;
		FieldType Type = FieldType::None;
		
		// If `Type` is `Enum` then this can be used to fetch valid `names - values`
		ScriptEnumFields EnumFields;

		PublicField() = default;
		PublicField(const std::string& name, const std::string& typeName, const std::string& toolTip, FieldType type);
		PublicField(std::string&& name, std::string&& typeName, std::string&& toolTip, FieldType type);
		PublicField(const PublicField& other);
		PublicField(PublicField&& other) noexcept = default;
		~PublicField();

		PublicField& operator= (const PublicField& other);
		PublicField& operator= (PublicField&& other) noexcept = default;

		bool operator< (const PublicField& other) const { return UIName < other.UIName; }

		void CopyStoredValueFromRuntime(MonoObject* instance);
		void CopyStoredValueToRuntime(MonoObject* instance) const;

		// Returns false if failed
		bool CopyStoredValue(const PublicField& other);
		bool IsStoredValueEqual(const PublicField& other);

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
			GetDataAsString().assign(value);
		}

		template<>
		const std::string& GetStoredValue() const
		{
			return GetDataAsString();
		}

		template <typename T>
		T GetRuntimeValue(MonoObject* instance) const
		{
			T value;
			GetRuntimeValue_Internal(instance, &value);
			return value;
		}

		template <>
		std::string GetRuntimeValue(MonoObject* instance) const
		{
			std::string value;
			GetRuntimeValue_Internal(instance, value);
			return value;
		}

		template <typename T>
		void SetRuntimeValue(MonoObject* instance, const T& value) const
		{
			if constexpr (std::is_same<std::string, T>::value)
			{
				SetRuntimeValue_Internal(instance, value);
			}
			else
			{
				void* ptr = (void*)&value; // Removing const because for some reason `mono` accepts non-const-ptr
				SetRuntimeValue_Internal(instance, ptr);
			}
		}

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

	private:
		void GetStoredValue_Internal(void* outValue) const
		{
			memcpy(outValue, m_StoredValueBuffer.Data(), m_StoredValueBuffer.Size());
		}

		void SetStoredValue_Internal(const void* value)
		{
			m_StoredValueBuffer.Write(value, m_StoredValueBuffer.Size());
		}

		void SetRuntimeValue_Internal(MonoObject* instance, void* value) const;
		void SetRuntimeValue_Internal(MonoObject* instance, const std::string& value) const;
		void GetRuntimeValue_Internal(MonoObject* instance, void* outValue) const;
		void GetRuntimeValue_Internal(MonoObject* instance, std::string& outValue) const;

		void AllocateBuffer(FieldType type)
		{
			uint32_t size = GetFieldSize(type);
			m_StoredValueBuffer.Allocate(size);
			if (type == FieldType::String)
			{
				new (m_StoredValueBuffer.Data()) std::string();
			}
			else
			{
				memset(m_StoredValueBuffer.Data(), 0, size);
			}
		}

		std::string& GetDataAsString()
		{
			return *(std::string*)(m_StoredValueBuffer.Data());
		}

		const std::string& GetDataAsString() const
		{
			return *(std::string*)(m_StoredValueBuffer.Data());
		}

	private:
		MonoClassField* m_MonoClassField = nullptr;
		MonoProperty* m_MonoProperty = nullptr;
		ScopedDataBuffer m_StoredValueBuffer;

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
