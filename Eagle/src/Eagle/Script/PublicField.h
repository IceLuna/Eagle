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
	struct EntityInstance;

	// `Enum value` -> it's name
	using ScriptEnumFields = std::map<int, std::string>;

	//Add new type to Scene Serializer
	enum class FieldType : uint32_t
	{
		None, Int, UnsignedInt, Float, String, Vec2, Vec3, Vec4, ClassReference,
		Bool, Color3, Color4, Enum, Entity,
		Asset, AssetTexture2D, AssetTextureCube, AssetStaticMesh, AssetSkeletalMesh, AssetAudio, AssetSoundGroup,
		AssetFont, AssetMaterial, AssetPhysicsMaterial, AssetEntity, AssetScene, AssetAnimation, AssetAnimationGraph,
		AssetParticleSystem, AssetAnimationBlendSpace,
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
				return true;
			default:
				return false;
		}
	}

	class PublicField
	{
	public:
		std::string Name;
		std::string TypeName;
		FieldType Type;
		
		// If `Type` is `Enum` then this can be used to fetch valid `names - values`
		ScriptEnumFields EnumFields;

		bool IsReadOnly = false;

		PublicField() = default;
		PublicField(const std::string& name, const std::string& typeName, FieldType type, bool isReadOnly = false);
		PublicField(const PublicField& other);
		PublicField(PublicField&& other) noexcept = default;
		~PublicField();

		PublicField& operator= (const PublicField& other);
		PublicField& operator= (PublicField&& other) noexcept = default;

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
			GetDataAsString().assign(value);
		}

		template<>
		const std::string& GetStoredValue() const
		{
			return GetDataAsString();
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
		void SetRuntimeValue(EntityInstance& entityInstance, const T& value)
		{
			if constexpr (std::is_same<std::string, T>::value)
			{
				SetRuntimeValue_Internal(entityInstance, value);
			}
			else
			{
				void* ptr = (void*)&value; // Removing const because for some reason `mono` accepts non-const-ptr
				SetRuntimeValue_Internal(entityInstance, ptr);
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
			if (IsReadOnly)
				return;

			m_StoredValueBuffer.Write(value, m_StoredValueBuffer.Size());
		}

		void SetRuntimeValue_Internal(EntityInstance& entityInstance, void* value);
		void SetRuntimeValue_Internal(EntityInstance& entityInstance, const std::string& value);
		void GetRuntimeValue_Internal(EntityInstance& entityInstance, void* outValue) const;
		void GetRuntimeValue_Internal(EntityInstance& entityInstance, std::string& outValue) const;

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