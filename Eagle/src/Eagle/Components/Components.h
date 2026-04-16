#pragma once

#include "SceneComponent.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Core/ScriptableEntity.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Camera/SceneCamera.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Script/PublicField.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsMaterial.h"
#include "Eagle/Physics/PhysicsEngine.h"
#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Audio/Sound3D.h"
#include "Eagle/Audio/Reverb3D.h"
#include "Eagle/Classes/Font.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/ParticleEmitter.h"
#include "Eagle/AI/NavigationMesh.h"

// If new component class is created, other changes are required:
// 1) Add new line into Scene's copy constructor;
// 2) Add new line into Scene::CopyComponents function;
// 3) Make it serializable;
// 4) Add it to EntityPropertiesPanel to draw UI (optional)
// 5) Add to ScriptEngineRegistry (optional)

namespace Eagle
{
	class BoxColliderShape;
	class SphereColliderShape;
	class CapsuleColliderShape;
	class MeshShape;
	class PhysicsActor;
	class PhysicsRagdollActor;

	class IDComponent
	{
	public:
		IDComponent() = default;
		IDComponent(const GUID& other) : ID(other) {}

		GUID ID;
	};

	class OwnershipComponent : public Component
	{
	public:
		OwnershipComponent(const Entity& entity) : Component(entity) {}

		OwnershipComponent& operator= (const OwnershipComponent& other)
		{
			if (this == &other)
				return *this;

			Component::operator=(other);

			const Scene* srcScene = other.Parent.GetScene();
			const Scene* destScene = Parent.GetScene();

			EG_CORE_ASSERT(srcScene, "Empty src Scene");
			EG_CORE_ASSERT(destScene, "Empty dest Scene");
			EG_CORE_ASSERT(srcScene != destScene, "Scene's are equal");
			
			Children.clear();
			for (auto& srcChild : other.Children)
			{
				Entity destChild = destScene->GetEntityByGUID(srcChild.GetComponent<IDComponent>().ID);
				destChild.SetParent(Parent);
			}

			return *this;
		}

		OwnershipComponent(OwnershipComponent&&) = default;
		OwnershipComponent& operator= (OwnershipComponent&&) = default;

		Entity EntityParent = Entity::Null;
		std::vector<Entity> Children;
	};

	class EntitySceneNameComponent
	{
	public:
		EntitySceneNameComponent() = default;
		EntitySceneNameComponent(const std::string& name) : Name(name) {}

		std::string Name;
	};

	// Internal component. It's used to indicate that an entity was created from AssetEntity.
	// So each entity, that was created from an asset, has this component.
	// It's used to get update entities when an asset changes.
	class EntityAssetComponent
	{
	public:
		EntityAssetComponent() = default;

		GUID AssetGUID = GUID(0, 0);
	};

	class TransformComponent
	{
	public:
		TransformComponent() = default;
		Transform WorldTransform;
		Transform RelativeTransform;
	};

	class TagComponent
	{
	public:
		std::string Tag;
	};

	class LightComponent : public SceneComponent
	{
	public:
		LightComponent(const Entity& entity) : SceneComponent(entity) {}
		LightComponent(const Entity& entity, const glm::vec3& lightColor) : SceneComponent(entity), m_LightColor(lightColor) {}
		COMPONENT_DEFAULTS(LightComponent);

		const glm::vec3& GetLightColor() const { return m_LightColor; }
		bool DoesAffectWorld() const { return m_bAffectsWorld; }
		float GetIntensity() const { return m_Intensity; }
		float GetVolumetricFogIntensity() const { return m_VolumetricFogIntensity; }
		bool DoesCastShadows() const { return m_bCastsShadows; }
		bool IsVolumetricLight() const { return m_bVolumetricLight; }

		virtual void SetLightColor(const glm::vec3& lightColor)
		{
			m_LightColor = lightColor;
		}

		virtual void SetAffectsWorld(bool bAffects)
		{
			m_bAffectsWorld = bAffects;
		}

		virtual void SetIntensity(float intensity)
		{
			m_Intensity = glm::max(0.f, intensity);
		}

		virtual void SetVolumetricFogIntensity(float intensity)
		{
			m_VolumetricFogIntensity = glm::max(0.f, intensity);
		}

		virtual void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
		}

		virtual void SetIsVolumetricLight(bool bVolumetric)
		{
			m_bVolumetricLight = bVolumetric;
		}
		
	protected:
		glm::vec3 m_LightColor = glm::vec3(1.f);
		float m_Intensity = 1.f;
		float m_VolumetricFogIntensity = 1.f;
		bool m_bAffectsWorld = true;
		bool m_bCastsShadows = true;
		bool m_bVolumetricLight = false;
	};

	class PointLightComponent : public LightComponent
	{
	public:
		PointLightComponent(const Entity& entity) : LightComponent(entity) {}
		PointLightComponent(const PointLightComponent&) = delete;
		PointLightComponent(PointLightComponent&& other) = default;
		PointLightComponent& operator=(PointLightComponent&& other) = default;

		PointLightComponent& operator=(const PointLightComponent& other)
		{
			if (this == &other)
				return *this;

			LightComponent::operator=(other);
			m_Radius = other.m_Radius;
			m_VisualizeRadiusEnabled = other.m_VisualizeRadiusEnabled;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnDebugStateChanged);
			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			LightComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			LightComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnTransformChanged);
		}

		virtual void SetLightColor(const glm::vec3& lightColor) override
		{
			m_LightColor = lightColor;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetAffectsWorld(bool bAffects) override
		{
			m_bAffectsWorld = bAffects;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetIntensity(float intensity) override
		{
			m_Intensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetVolumetricFogIntensity(float intensity) override
		{
			m_VolumetricFogIntensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		void SetRadius(float radius)
		{
			m_Radius = glm::max(0.f, radius);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		float GetRadius() const { return m_Radius; }

		void SetVisualizeRadiusEnabled(bool bEnabled)
		{
			if (m_VisualizeRadiusEnabled != bEnabled)
			{
				m_VisualizeRadiusEnabled = bEnabled;
				Parent.SignalComponentChanged<PointLightComponent>(Notification::OnDebugStateChanged);
			}
		}
		
		bool VisualizeRadiusEnabled() const { return m_VisualizeRadiusEnabled; }

		void SetCastsShadows(bool bCasts) override
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		void SetIsVolumetricLight(bool bVolumetric) override
		{
			m_bVolumetricLight = bVolumetric;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

	private:
		float m_Radius = 1.f;
		bool m_VisualizeRadiusEnabled = false;
	};

	class DirectionalLightComponent : public LightComponent
	{
	public:
		DirectionalLightComponent(const Entity& entity) : LightComponent(entity) {}
		DirectionalLightComponent(const Entity& entity, const glm::vec3& lightColor)
			: LightComponent(entity, lightColor) {}

		DirectionalLightComponent(const DirectionalLightComponent&) = delete;
		DirectionalLightComponent(DirectionalLightComponent&& other) = default;
		DirectionalLightComponent& operator=(DirectionalLightComponent&& other) = default;
		DirectionalLightComponent& operator=(const DirectionalLightComponent& other)
		{
			if (this == &other)
				return *this;

			LightComponent::operator=(other);

			m_Ambient = other.m_Ambient;
			bVisualizeDirection = other.bVisualizeDirection;
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnDebugStateChanged);
			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			LightComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			LightComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnTransformChanged);
		}

		void SetLightColor(const glm::vec3& lightColor) override
		{
			m_LightColor = lightColor;
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
		}

		void SetAffectsWorld(bool bAffects) override
		{
			m_bAffectsWorld = bAffects;
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
		}

		void SetIntensity(float intensity) override
		{
			m_Intensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
		}

		void SetVolumetricFogIntensity(float intensity) override
		{
			m_VolumetricFogIntensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
		}

		void SetAmbientColor(const glm::vec3& ambient)
		{
			m_Ambient = ambient;
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
		}

		const glm::vec3& GetAmbientColor() const { return m_Ambient; }

		void SetVisualizeDirectionEnabled(bool bEnabled)
		{
			if (bVisualizeDirection != bEnabled)
			{
				bVisualizeDirection = bEnabled;
				Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnDebugStateChanged);
			}
		}

		bool IsVisualizeDirectionEnabled() const { return bVisualizeDirection; }

		void SetCastsShadows(bool bCasts) override
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
		}

		void SetIsVolumetricLight(bool bVolumetric) override
		{
			m_bVolumetricLight = bVolumetric;
			Parent.SignalComponentChanged<DirectionalLightComponent>(Notification::OnStateChanged);
		}

	private:
		glm::vec3 m_Ambient = glm::vec3(0.f);
		bool bVisualizeDirection = false;
	};

	class SpotLightComponent : public LightComponent
	{
	public:
		SpotLightComponent(const Entity& entity) : LightComponent(entity) {}

		SpotLightComponent(const SpotLightComponent&) = delete;
		SpotLightComponent(SpotLightComponent&& other) = default;
		SpotLightComponent& operator=(SpotLightComponent&& other) = default;

		SpotLightComponent& operator=(const SpotLightComponent& other)
		{
			if (this == &other)
				return *this;

			LightComponent::operator=(other);
			m_InnerCutOffAngle = other.m_InnerCutOffAngle;
			m_OuterCutOffAngle = other.m_OuterCutOffAngle;
			m_Distance = other.m_Distance;
			m_VisualizeDistanceEnabled = other.m_VisualizeDistanceEnabled;

			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnDebugStateChanged);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			LightComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			LightComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnTransformChanged);
		}

		void SetLightColor(const glm::vec3& lightColor) override
		{
			m_LightColor = lightColor;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetAffectsWorld(bool bAffects) override
		{
			m_bAffectsWorld = bAffects;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetIntensity(float intensity) override
		{
			m_Intensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetVolumetricFogIntensity(float intensity) override
		{
			m_VolumetricFogIntensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		float GetInnerCutOffAngle() const { return m_InnerCutOffAngle; }
		
		void SetInnerCutOffAngle(float angle)
		{
			angle = glm::clamp(angle, 1.f, 80.f);

			m_InnerCutOffAngle = std::min(m_OuterCutOffAngle, angle);
			m_OuterCutOffAngle = std::max(m_OuterCutOffAngle, angle);

			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		float GetOuterCutOffAngle() const { return m_OuterCutOffAngle; }
		
		void SetOuterCutOffAngle(float angle)
		{
			angle = glm::clamp(angle, 1.f, 80.f);

			m_OuterCutOffAngle = std::max(m_InnerCutOffAngle, angle);
			m_InnerCutOffAngle = std::min(m_InnerCutOffAngle, angle);

			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetDistance(float distance)
		{
			m_Distance = glm::max(0.f, distance);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		float GetDistance() const { return m_Distance; }

		void SetVisualizeDistanceEnabled(bool bEnabled)
		{
			if (m_VisualizeDistanceEnabled != bEnabled)
			{
				m_VisualizeDistanceEnabled = bEnabled;
				Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnDebugStateChanged);
			}
		}
		bool VisualizeDistanceEnabled() const { return m_VisualizeDistanceEnabled; }

		virtual void SetCastsShadows(bool bCasts) override
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetIsVolumetricLight(bool bVolumetric) override
		{
			m_bVolumetricLight = bVolumetric;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

	private:
		float m_InnerCutOffAngle = 25.f;
		float m_OuterCutOffAngle = 45.f;
		float m_Distance = 1.f;
		bool m_VisualizeDistanceEnabled = false;
	};

	class SpriteComponent : public SceneComponent
	{
	public:
		SpriteComponent(const Entity& entity) : SceneComponent(entity) {}
		SpriteComponent(const SpriteComponent&) = delete;
		SpriteComponent(SpriteComponent&&) noexcept = default;
		SpriteComponent& operator=(SpriteComponent&&) noexcept = default;

		SpriteComponent& operator=(const SpriteComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);

			m_MaterialAsset = other.m_MaterialAsset;
			m_SpriteCoords = other.m_SpriteCoords;
			m_SpriteSize = other.m_SpriteSize;
			m_SpriteSizeCoef = other.m_SpriteSizeCoef;
			bAtlas = other.bAtlas;
			m_bCastsShadows = other.m_bCastsShadows;
			m_bReceivesDecals = other.m_bReceivesDecals;
			m_bVisible = other.m_bVisible;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);

			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnTransformChanged);
		}

		void SetIsAtlas(bool value)
		{
			bAtlas = value;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}
		
		bool IsAtlas() const { return bAtlas; }

		void SetAtlasSpriteCoords(glm::vec2 coords)
		{
			m_SpriteCoords = coords;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}

		glm::vec2 GetAtlasSpriteCoords() const { return m_SpriteCoords; }

		void SetAtlasSpriteSize(glm::vec2 size)
		{
			m_SpriteSize = size;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}

		glm::vec2 GetAtlasSpriteSize() const { return m_SpriteSize; }

		void SetAtlasSpriteSizeCoef(glm::vec2 sizeCoef)
		{
			m_SpriteSizeCoef = sizeCoef;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}

		glm::vec2 GetAtlasSpriteSizeCoef() const { return m_SpriteSizeCoef; }

		const Ref<AssetMaterial>& GetMaterialAsset() const { return m_MaterialAsset; }
		
		void SetMaterialAsset(const Ref<AssetMaterial>& material)
		{
			m_MaterialAsset = material;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnMaterialChanged);
		}

		void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}
		
		bool DoesCastShadows() const { return m_bCastsShadows; }

		void SetReceivesDecals(bool bReceives)
		{
			m_bReceivesDecals = bReceives;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}
		bool DoesReceiveDecals() const { return m_bReceivesDecals; }

		void SetVisible(bool bVisible)
		{
			m_bVisible = bVisible;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}
		bool IsVisible() const { return m_bVisible; }

	private:
		Ref<AssetMaterial> m_MaterialAsset;
		
		// Atlas params
		glm::vec2 m_SpriteCoords = { 0, 0 };
		glm::vec2 m_SpriteSize = { 64, 64 };
		glm::vec2 m_SpriteSizeCoef = { 1, 1 };

		bool bAtlas = false;
		bool m_bCastsShadows = true;
		bool m_bReceivesDecals = true;
		bool m_bVisible = true;
	};

	class StaticMeshComponent : public SceneComponent
	{
	public:
		StaticMeshComponent(const Entity& entity) : SceneComponent(entity) {}
		StaticMeshComponent(const StaticMeshComponent&) = delete;
		StaticMeshComponent(StaticMeshComponent&& other) = default;
		StaticMeshComponent& operator=(StaticMeshComponent&& other) = default;

		StaticMeshComponent& operator=(const StaticMeshComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);

			m_MeshAsset = other.m_MeshAsset;
			m_MaterialAssets = other.m_MaterialAssets;
			m_bCastsShadows = other.m_bCastsShadows;
			m_bReceivesDecals = other.m_bReceivesDecals;
			m_bVisible = other.m_bVisible;

			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
			return *this;
		}

		const Ref<AssetStaticMesh>& GetMeshAsset() const { return m_MeshAsset; }
		void SetMeshAsset(const Ref<AssetStaticMesh>& mesh)
		{
			m_MeshAsset = mesh;
			if (m_MeshAsset)
			{
				const auto& mesh = m_MeshAsset->GetMesh();
				const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
				m_MaterialAssets.resize(materialsCount);
				for (uint32_t i = 0; i < materialsCount; ++i)
					m_MaterialAssets[i] = mesh->GetMaterialAsset(i);
			}
			else
				m_MaterialAssets.clear();

			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnTransformChanged);
		}

		void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
		}
		bool DoesCastShadows() const { return m_bCastsShadows; }

		uint32_t GetMaterialsSlotsCount() const { return (uint32_t)m_MaterialAssets.size(); }
		const Ref<AssetMaterial>& GetMaterialAsset(uint32_t index) const { return m_MaterialAssets[index]; }
		void SetMaterialAsset(uint32_t index, const Ref<AssetMaterial>& material)
		{
			if (index >= m_MaterialAssets.size())
				return;
			m_MaterialAssets[index] = material;
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnMaterialChanged);
		}

		void SetReceivesDecals(bool bReceives)
		{
			m_bReceivesDecals = bReceives;
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
		}
		bool DoesReceiveDecals() const { return m_bReceivesDecals; }

		void SetVisible(bool bVisible)
		{
			m_bVisible = bVisible;
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
		}
		bool IsVisible() const { return m_bVisible; }

	private:
		Ref<AssetStaticMesh> m_MeshAsset;
		std::vector<Ref<AssetMaterial>> m_MaterialAssets;
		bool m_bCastsShadows = true;
		bool m_bReceivesDecals = true;
		bool m_bVisible = true;
	};

	class SkeletalMeshComponent : public SceneComponent
	{
	public:
		SkeletalMeshComponent(const Entity& entity) : SceneComponent(entity) {}
		~SkeletalMeshComponent();
		SkeletalMeshComponent(const SkeletalMeshComponent&) = delete;
		SkeletalMeshComponent(SkeletalMeshComponent&& other) = default;
		SkeletalMeshComponent& operator=(SkeletalMeshComponent&& other) = default;

		SkeletalMeshComponent& operator=(const SkeletalMeshComponent& other);

		const Ref<AssetSkeletalMesh>& GetMeshAsset() const { return m_MeshAsset; }
		void SetMeshAsset(const Ref<AssetSkeletalMesh>& mesh);

		const Ref<AssetAnimation>& GetAnimationAsset() const { return m_AnimAsset; }
		void SetAnimationAsset(const Ref<AssetAnimation>& anim)
		{
			m_AnimAsset = anim;
			CurrentClipPlayTime = 0.f;
			PrevClipPlayTime = 0.f;
		}

		const Ref<AssetAnimationGraph>& GetAnimationGraphAsset() const { return m_AnimGraphAsset; }
		void SetAnimationGraphAsset(const Ref<AssetAnimationGraph>& anim);

		const Ref<AnimationGraph>& GetAnimationGraph() const { return m_Graph; }

		void SetWorldTransform(const Transform& worldTransform) override;
		void SetRelativeTransform(const Transform& relativeTransform) override;

		void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnStateChanged);
		}
		bool DoesCastShadows() const { return m_bCastsShadows; }

		void SetReceivesDecals(bool bReceives)
		{
			m_bReceivesDecals = bReceives;
			Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnStateChanged);
		}
		bool DoesReceiveDecals() const { return m_bReceivesDecals; }

		void SetVisible(bool bVisible)
		{
			m_bVisible = bVisible;
			Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnStateChanged);
		}
		bool IsVisible() const { return m_bVisible; }

		uint32_t GetMaterialsSlotsCount() const { return (uint32_t)m_MaterialAssets.size(); }
		const Ref<AssetMaterial>& GetMaterialAsset(uint32_t index) const { return m_MaterialAssets[index]; }
		void SetMaterialAsset(uint32_t index, const Ref<AssetMaterial>& material)
		{
			if (index >= m_MaterialAssets.size())
				return;
			m_MaterialAssets[index] = material;
			Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnMaterialChanged);
		}

		bool HasBone(const std::string_view boneName) const;
		Transform GetBoneWorldTransform(const std::string_view boneName) const;
		glm::vec3 GetBoneWorldLocation(const std::string_view boneName) const;
		Rotator GetBoneWorldRotation(const std::string_view boneName) const;
		glm::vec3 GetBoneWorldScale(const std::string_view boneName) const;

		bool IsRootMotionLockFlagSet(RootMotionLockFlag flag) const { return HasFlags(m_RootMotionLockFlags, flag); }
		void SetRootMotionLockFlag(RootMotionLockFlag flag, bool value) { SetFlag(m_RootMotionLockFlags, flag, value); }
		void SetRootMotionLockFlag(RootMotionLockFlag flag) { m_RootMotionLockFlags = flag; }
		RootMotionLockFlag GetRootMotionLockFlags() const { return m_RootMotionLockFlags; }

		void SetRagdollEnabled(bool bEnabled);
		bool IsRagdollEnabled() const { return m_bRagdollEnabled; }
		const Ref<PhysicsRagdollActor>& GetRagdollActor() const { return m_RagdollActor; }
		Ref<PhysicsRagdollActor>& GetRagdollActor() { return m_RagdollActor; }

		bool IsRagdollCollisionShown() const;
		void SetShowRagdollCollision(bool bShow);
		Transform GetRagdollBoneWorldTransform(const std::string& name) const;
		Transform GetRagdollRootBoneWorldTransform() const;

		// Update all bones
		void SetRagdollLinearVelocity(const glm::vec3& velocity, bool bApplyToRootOnly);
		void SetRagdollAngularVelocity(const glm::vec3& velocity, bool bApplyToRootOnly);

		void SetRagdollBoneLinearVelocity(const std::string& boneName, const glm::vec3& velocity);
		void SetRagdollBoneAngularVelocity(const std::string& boneName, const glm::vec3& velocity);
		glm::vec3 GetRagdollBoneLinearVelocity(const std::string& boneName) const;
		glm::vec3 GetRagdollBoneAngularVelocity(const std::string& boneName) const;

		void PutRagdollToSleep();
		void WakeUpRagdoll();

	public:
		SkeletalPose LastPose; // The final pose that was calculated during the last animation update

		// These are used only if `AnimType` == `AnimationType::Clip`
		float CurrentClipPlayTime = 0.f;
		float PrevClipPlayTime = 0.f;
		float ClipPlaybackSpeed = 1.f;
		float PrevClipPlaybackSpeed = 1.f;
		bool bClipLooping = true;

		AnimationType AnimType = AnimationType::Clip;

	private:
		SkeletalPose m_PreRagdollLastPose; // Pose the mesh had before ragdoll was enabled
		Ref<AssetSkeletalMesh> m_MeshAsset;
		std::vector<Ref<AssetMaterial>> m_MaterialAssets;
		Ref<AssetAnimation> m_AnimAsset;
		Ref<AssetAnimationGraph> m_AnimGraphAsset;
		Ref<AnimationGraph> m_Graph;
		Ref<PhysicsRagdollActor> m_RagdollActor;
		GUID m_CallbackID;
		RootMotionLockFlag m_RootMotionLockFlags = RootMotionLockFlag::None;
		bool m_bCastsShadows = true;
		bool m_bReceivesDecals = true;
		bool m_bVisible = true;
		bool m_bRagdollEnabled = false;
	};

	class BillboardComponent : public SceneComponent
	{
	public:
		BillboardComponent(const Entity& entity) : SceneComponent(entity) {}
		COMPONENT_DEFAULTS(BillboardComponent);

		Ref<AssetTexture2D> TextureAsset;
		bool bVisible = true;
	};

	class Image2DComponent : public Component
	{
	public:
		Image2DComponent(const Entity& entity) : Component(entity) {}
		COMPONENT_DEFAULTS(Image2DComponent);

		void SetTextureAsset(const Ref<AssetTexture2D>& asset)
		{
			m_TextureAsset = asset;
			Parent.SignalComponentChanged<Image2DComponent>(Notification::OnStateChanged);
		}

		void SetTint(const glm::vec3& tint)
		{
			m_Tint = tint;
			Parent.SignalComponentChanged<Image2DComponent>(Notification::OnStateChanged);
		}

		void SetPosition(glm::vec2 pos)
		{
			m_Pos = pos;
			Parent.SignalComponentChanged<Image2DComponent>(Notification::OnStateChanged);
		}

		void SetScale(glm::vec2 scale)
		{
			m_Scale = scale;
			Parent.SignalComponentChanged<Image2DComponent>(Notification::OnStateChanged);
		}

		void SetRotation(float rotationZ)
		{
			m_Rotation = rotationZ;
			Parent.SignalComponentChanged<Image2DComponent>(Notification::OnStateChanged);
		}

		void SetOpacity(float opacity)
		{
			m_Opacity = glm::clamp(opacity, 0.f, 1.f);
			Parent.SignalComponentChanged<Image2DComponent>(Notification::OnStateChanged);
		}

		void SetIsVisible(bool bVisible)
		{
			m_IsVisible = bVisible;
			Parent.SignalComponentChanged<Image2DComponent>(Notification::OnStateChanged);
		}

		const Ref<AssetTexture2D>& GetTextureAsset() const { return m_TextureAsset; }
		glm::vec3 GetTint() const { return m_Tint; }
		glm::vec2 GetPosition() const { return m_Pos; }
		glm::vec2 GetScale() const { return m_Scale; }
		float GetRotation() const { return m_Rotation; }
		float GetOpacity() const { return m_Opacity; }
		bool IsVisible() const { return m_IsVisible; }

	private:
		Ref<AssetTexture2D> m_TextureAsset;
		glm::vec3 m_Tint = glm::vec3(1.f);
		glm::vec2 m_Pos = glm::vec2{ 0.0f }; // Normalized device coords
		glm::vec2 m_Scale = glm::vec2{ 0.5f };
		float m_Rotation = 0.f;
		float m_Opacity = 1.f;
		bool m_IsVisible = true;
	};

	class TextComponent : public SceneComponent
	{
	public:
		TextComponent(const Entity& entity) : SceneComponent(entity) {}
		COMPONENT_DEFAULTS(TextComponent);

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnTransformChanged);
		}

		const Ref<AssetFont>& GetFontAsset() const { return m_FontAsset; }
		const Ref<AssetMaterial>& GetMaterialAsset() const { return m_MaterialAsset; }
		const std::string& GetText() const { return m_Text; }
		const glm::vec3& GetColor() const { return m_Color; }
		float GetLineSpacing() const { return m_LineSpacing; }
		float GetKerning() const { return m_Kerning; }
		float GetMaxWidth() const { return m_MaxWidth; }
		bool IsLit() const { return m_bLit; }

		void SetFontAsset(const Ref<AssetFont>& font)
		{
			m_FontAsset = font;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetText(const std::string& text)
		{
			m_Text = text;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetColor(const glm::vec3& color)
		{
			m_Color = color;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetLineSpacing(float value)
		{
			m_LineSpacing = value;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetKerning(float value)
		{
			m_Kerning = value;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetMaxWidth(float value)
		{
			m_MaxWidth = glm::max(value, 0.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetIsLit(bool bLit)
		{
			m_bLit = bLit;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetMaterialAsset(const Ref<AssetMaterial>& material)
		{
			m_MaterialAsset = material;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnMaterialChanged);
		}

		void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}
		bool DoesCastShadows() const { return m_bCastsShadows; }

		void SetReceivesDecals(bool bReceives)
		{
			m_bReceivesDecals = bReceives;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}
		bool DoesReceiveDecals() const { return m_bReceivesDecals; }

		void SetVisible(bool bVisible)
		{
			m_bVisible = bVisible;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}
		bool IsVisible() const { return m_bVisible; }

		void SetDoubleSided(bool bDoubleSided)
		{
			m_bDoubleSided = bDoubleSided;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}
		bool IsDoubleSided() const { return m_bDoubleSided; }

	private:
		std::string m_Text = "Hello, World!";
		Ref<AssetFont> m_FontAsset;
		Ref<AssetMaterial> m_MaterialAsset; // Used if bLit == true

		glm::vec3 m_Color = glm::vec3(1.f); // Used if bLit == false
		float m_LineSpacing = 0.0f;
		float m_Kerning = 0.0f;
		float m_MaxWidth = 10.0f;

		bool m_bDoubleSided = false; // Used if bLit == false
		bool m_bLit = false;
		bool m_bCastsShadows = false;
		bool m_bReceivesDecals = true;
		bool m_bVisible = true;
	};

	class Text2DComponent : public Component
	{
	public:
		Text2DComponent(const Entity& entity) : Component(entity) {}
		COMPONENT_DEFAULTS(Text2DComponent);

		void SetFontAsset(const Ref<AssetFont>& font)
		{
			m_FontAsset = font;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetText(const std::string& text)
		{
			m_Text = text;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetColor(const glm::vec3& color)
		{
			m_Color = color;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetLineSpacing(float value)
		{
			m_LineSpacing = value;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetKerning(float value)
		{
			m_Kerning = value;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetMaxWidth(float value)
		{
			m_MaxWidth = glm::max(value, 0.f);
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetPosition(glm::vec2 pos)
		{
			m_Pos = pos;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetScale(glm::vec2 scale)
		{
			m_Scale = scale;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetRotation(float rotationZ)
		{
			m_Rotation = rotationZ;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetOpacity(float opacity)
		{
			m_Opacity = glm::clamp(opacity, 0.f, 1.f);
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetIsVisible(bool bVisible)
		{
			m_IsVisible = bVisible;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		const Ref<AssetFont>& GetFontAsset() const { return m_FontAsset; }
		const std::string& GetText() const { return m_Text; }
		const glm::vec3& GetColor() const { return m_Color; }
		float GetLineSpacing() const { return m_LineSpacing; }
		float GetKerning() const { return m_Kerning; }
		float GetMaxWidth() const { return m_MaxWidth; }
		glm::vec2 GetPosition() const { return m_Pos; }
		glm::vec2 GetScale() const { return m_Scale; }
		float GetRotation() const { return m_Rotation; }
		float GetOpacity() const { return m_Opacity; }
		bool IsVisible() const { return m_IsVisible; }

	private:
		std::string m_Text = "Hello, 2D World!";
		Ref<AssetFont> m_FontAsset;

		glm::vec3 m_Color = glm::vec3(1.f);
		float m_LineSpacing = 0.0f;
		glm::vec2 m_Pos = glm::vec2{ 0.0f }; // Normalized device coords
		glm::vec2 m_Scale = glm::vec2{ 0.5f };
		float m_Rotation = 0.f;
		float m_Kerning = 0.0f;
		float m_MaxWidth = 10.0f;
		float m_Opacity = 1.f;
		bool m_IsVisible = true;
	};

	class CameraComponent : public SceneComponent
	{
	public:
		CameraComponent(const Entity& entity) : SceneComponent(entity) {}
		CameraComponent(const CameraComponent&) = delete;
		CameraComponent(CameraComponent&&) noexcept = default;
		CameraComponent& operator=(CameraComponent&&) noexcept = default;

		CameraComponent& operator=(const CameraComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);

			m_ViewMatrix = other.m_ViewMatrix;
			bDebugFrustumCulling = other.bDebugFrustumCulling;
			Camera = other.Camera;
			Primary = other.Primary;
			FixedAspectRatio = other.FixedAspectRatio;

			Parent.SignalComponentChanged<CameraComponent>(Notification::OnStateChanged);
			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			CalculateViewMatrix();
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			CalculateViewMatrix();
		}

		glm::mat4 GetViewProjection() const
		{
			return Camera.GetProjection() * GetViewMatrix();
		}
		
		const glm::mat4& GetViewMatrix() const
		{
			return m_ViewMatrix;
		}

		void SetDebugFrustumCullingEnabled(bool bEnabled)
		{
			bDebugFrustumCulling = bEnabled;
			Parent.SignalComponentChanged<CameraComponent>(Notification::OnDebugStateChanged);
		}

		bool IsDebugFrustumCullingEnabled() const { return bDebugFrustumCulling; }
		
	private:
		void CalculateViewMatrix()
		{
			const glm::mat4 R = WorldTransform.Rotation.ToMat4();
			const glm::mat4 T = glm::translate(glm::mat4(1.0f), WorldTransform.Location);
			m_ViewMatrix = T * R;
			m_ViewMatrix = glm::inverse(m_ViewMatrix);
		}

	private:
		glm::mat4 m_ViewMatrix = glm::mat4(1.f);
		bool bDebugFrustumCulling = false; // When enabled, this camera's frustum will be used for culling

	public:
		SceneCamera Camera;
		bool Primary = false; //TODO: think about moving to Scene, or somewhere else
		bool FixedAspectRatio = false;
	};

	class RigidBodyComponent : public Component
	{
	public:
		RigidBodyComponent(const Entity& entity, PhysicsBodyType bodyType = PhysicsBodyType::Static) : Component(entity), m_BodyType(bodyType) {}
		COMPONENT_DEFAULTS(RigidBodyComponent);

		void SetBodyType(PhysicsBodyType type);
		PhysicsBodyType GetBodyType() const { return m_BodyType; }

		void SetCollisionDetectionType(CollisionDetectionType type);
		CollisionDetectionType GetCollisionDetectionType() const { return m_CollisionDetection; }

		/*
			The solver iteration count determines how accurately joints and contacts are resolved.
			If you are having trouble with jointed bodies oscillating and behaving erratically, then
			setting a higher position iteration count may improve their stability. Range: [1, 255]

			If intersecting bodies are being depenetrated too violently, increase the number of velocity
			iterations.More velocity iterations will drive the relative exit velocity of the intersecting
			objects closer to the correct value given the restitution. Range: [0, 255]
		*/
		void SetPositionSolverIterations(uint32_t iterations);
		void SetVelocitySolverIterations(uint32_t iterations);
		uint32_t GetPositionSolverIterations() const { return PositionSolverIterations; }
		uint32_t GetVelocitySolverIterations() const { return VelocitySolverIterations; }

		void SetMass(float mass);
		float GetMass() const { return Mass; }

		Ref<PhysicsActor>& GetPhysicsActor() { return Parent.GetPhysicsActor(); }
		const Ref<PhysicsActor>& GetPhysicsActor() const { return Parent.GetPhysicsActor(); }

		void SetLinearDamping(float linearDamping);
		float GetLinearDamping() const { return LinearDamping; }

		void SetAngularDamping(float angularDamping);
		float GetAngularDamping() const { return AngularDamping; }

		void SetEnableGravity(bool bEnable);
		bool IsGravityEnabled() const { return bEnableGravity; }

		void SetMaxLinearVelocity(float velocity);
		float GetMaxLinearVelocity() const { return MaxLinearVelocity; }

		void SetMaxAngularVelocity(float velocity);
		float GetMaxAngularVelocity() const { return MaxAngularVelocity; }

		void SetIsKinematic(bool bKinematic);
		bool IsKinematic() const { return bKinematic; }

		void WakeUp();

		bool IsLockFlagSet(ActorLockFlag flag) const { return HasFlags(m_LockFlags, flag); }
		void SetLockFlag(ActorLockFlag flag, bool value);
		void SetLockFlag(ActorLockFlag flag);
		ActorLockFlag GetLockFlags() const { return m_LockFlags; }

	protected:
		PhysicsBodyType m_BodyType = PhysicsBodyType::Static;
		CollisionDetectionType m_CollisionDetection = CollisionDetectionType::Discrete;
		uint32_t PositionSolverIterations = 4; // [1; 255]
		uint32_t VelocitySolverIterations = 1; // [0; 255]
		float Mass = 1.f;
		float LinearDamping = 0.01f;
		float AngularDamping = 0.05f;
		float MaxLinearVelocity = 10000.f;
		float MaxAngularVelocity = 10000.f;
		bool bEnableGravity = false;
		bool bKinematic = false;
		
		ActorLockFlag m_LockFlags = ActorLockFlag::None;
	};

	class BaseColliderComponent : public SceneComponent
	{
	public:
		virtual void SetIsTrigger(bool bTrigger) = 0;
		bool IsTrigger() const { return bTrigger; }

		void SetPhysicsMaterialAsset(const Ref<AssetPhysicsMaterial>& material);
		const Ref<AssetPhysicsMaterial>& GetPhysicsMaterialAsset() const { return m_MaterialAsset; }

		virtual void SetWorldTransform(const Transform& worldTransform) override;
		virtual void SetRelativeTransform(const Transform& relativeTransform) override;

		bool IsCollisionVisible() const { return bShowCollision; }
		virtual void SetShowCollision(bool bShowCollision) = 0;

		bool IsCollisionEnabled() const { return bCollisionEnabled; }
		virtual void SetCollisionEnabled(bool bEnabled) = 0;

		void SetAffectsNavMeshBuild(bool bAffects) { bAffectsNavMeshBuild = bAffects; }
		bool DoesAffectNavMeshBuild() const { return bAffectsNavMeshBuild; }

		bool IsObstacle() const { return bObstacle; }
		void SetIsObstacle(bool bValue);

		// Collision groups it belongs to. It can belong to different groups (use XOR to combine groups)
		virtual void SetCollisionGroup(CollisionGroup groups) = 0;
		CollisionGroup GetCollisionGroup() const { return m_CollisionGroup; }

		// Collision groups it can interact with
		virtual void SetInteractingCollisionGroup(CollisionGroup groups) = 0;
		CollisionGroup GetInteractingCollisionGroup() const { return m_InteractingCollisionGroup; }

	protected:
		BaseColliderComponent(const Entity& entity) : SceneComponent(entity){}
		BaseColliderComponent& operator=(const BaseColliderComponent& other);
		BaseColliderComponent(const BaseColliderComponent&) = delete;
		BaseColliderComponent(BaseColliderComponent&&) noexcept = default;
		BaseColliderComponent& operator=(BaseColliderComponent&&) noexcept = default;

		virtual void UpdatePhysicsTransform() = 0;
		virtual void UpdatePhysicsMaterials() = 0;
		virtual void CreateObstacle() {} // Should remove an obstacle if it's already created
		bool RemoveObstacle(); // Returns true if success. It might fail if there have been a lot of nav mesh change requests

	protected:
		Ref<AssetPhysicsMaterial> m_MaterialAsset;
		CollisionGroup m_CollisionGroup = s_DefaultCollisionGroup;
		CollisionGroup m_InteractingCollisionGroup = s_DefaultInteractingCollisionGroup;
		dtObstacleRef m_ObstacleID = 0u;
		bool bTrigger = false;
		bool bShowCollision = false;
		bool bAffectsNavMeshBuild = true; // If set to false, collider won't be used during the nav mesh build process
		bool bObstacle = false; // Can be used for NavMesh to dynamically block the path. Not supported by mesh colliders
		bool bCollisionEnabled = true;
	};

	class BoxColliderComponent : public BaseColliderComponent
	{
	public:
		BoxColliderComponent(const Entity& entity) : BaseColliderComponent(entity) { OnInit(); }
		BoxColliderComponent& operator=(const BoxColliderComponent& other);
		BoxColliderComponent(const BoxColliderComponent&) = delete;
		BoxColliderComponent(BoxColliderComponent&&) noexcept = default;
		BoxColliderComponent& operator=(BoxColliderComponent&&) noexcept = default;

		void OnInit();

		void SetIsTrigger(bool bTrigger) override;
		void SetShowCollision(bool bShowCollision) override;
		void OnRemoved() override;
		void SetCollisionGroup(CollisionGroup groups) override;
		void SetInteractingCollisionGroup(CollisionGroup groups) override;
		void SetCollisionEnabled(bool bEnabled) override;

		void SetSize(const glm::vec3& size);
		const glm::vec3& GetSize() const { return m_Size; }

		const Ref<BoxColliderShape>& GetShape() const { return m_Shape; }
	
	protected:
		void UpdatePhysicsTransform() override;
		void UpdatePhysicsMaterials() override;
		void CreateObstacle() override;

	protected:
		Ref<BoxColliderShape> m_Shape;
		glm::vec3 m_Size = glm::vec3(1.f);
	};

	class SphereColliderComponent : public BaseColliderComponent
	{
	public:
		SphereColliderComponent(const Entity& entity) : BaseColliderComponent(entity) { OnInit(); }
		SphereColliderComponent& operator=(const SphereColliderComponent& other);
		SphereColliderComponent(const SphereColliderComponent&) = delete;
		SphereColliderComponent(SphereColliderComponent&&) noexcept = default;
		SphereColliderComponent& operator=(SphereColliderComponent&&) noexcept = default;

		void OnInit();

		void SetRadius(float radius);
		float GetRadius() const { return m_Radius; }

		void SetIsTrigger(bool bTrigger) override;
		void SetShowCollision(bool bShowCollision) override;
		void SetCollisionGroup(CollisionGroup groups) override;
		void SetInteractingCollisionGroup(CollisionGroup groups) override;
		void SetCollisionEnabled(bool bEnabled) override;

		virtual void OnRemoved() override;

		const Ref<SphereColliderShape>& GetShape() const { return m_Shape; }
	
	protected:
		void UpdatePhysicsTransform() override;
		void UpdatePhysicsMaterials() override;
		void CreateObstacle() override;

	protected:
		Ref<SphereColliderShape> m_Shape;
		float m_Radius = 0.5f;
	};

	class CapsuleColliderComponent : public BaseColliderComponent
	{
	public:
		CapsuleColliderComponent(const Entity& entity) : BaseColliderComponent(entity) { OnInit(); }
		CapsuleColliderComponent& operator=(const CapsuleColliderComponent& other);
		CapsuleColliderComponent(const CapsuleColliderComponent&) = delete;
		CapsuleColliderComponent(CapsuleColliderComponent&&) noexcept = default;
		CapsuleColliderComponent& operator=(CapsuleColliderComponent&&) noexcept = default;

		void OnInit();

		void SetIsTrigger(bool bTrigger) override;
		void SetShowCollision(bool bShowCollision) override;
		void SetCollisionGroup(CollisionGroup groups) override;
		void SetInteractingCollisionGroup(CollisionGroup groups) override;
		void SetCollisionEnabled(bool bEnabled) override;

		void SetHeight(float height)
		{
			SetHeightAndRadius(height, m_Radius);
		}
		float GetHeight() const { return m_Height; }

		void SetRadius(float radius)
		{
			SetHeightAndRadius(m_Height, radius);
		}
		float GetRadius() const { return m_Radius; }

		void SetHeightAndRadius(float height, float radius);

		virtual void OnRemoved() override;

		const Ref<CapsuleColliderShape>& GetShape() const { return m_Shape; }

	protected:
		void UpdatePhysicsTransform() override;
		void UpdatePhysicsMaterials() override;
		void CreateObstacle() override;

	protected:
		Ref<CapsuleColliderShape> m_Shape;
		float m_Radius = 0.5f;
		float m_Height = 1.f;
	};

	class MeshColliderComponent : public BaseColliderComponent
	{
	public:
		MeshColliderComponent(const Entity& entity) : BaseColliderComponent(entity) { OnInit(); }
		MeshColliderComponent& operator=(const MeshColliderComponent& other);
		MeshColliderComponent(const MeshColliderComponent&) = delete;
		MeshColliderComponent(MeshColliderComponent&&) noexcept = default;
		MeshColliderComponent& operator=(MeshColliderComponent&&) noexcept = default;

		void OnInit();

		void SetIsTrigger(bool bTrigger) override;
		void SetShowCollision(bool bShowCollision) override;
		void SetCollisionGroup(CollisionGroup groups) override;
		void SetInteractingCollisionGroup(CollisionGroup groups) override;
		void SetCollisionEnabled(bool bEnabled) override;

		void SetCollisionMeshAsset(const Ref<AssetBaseMesh>& meshAsset);
		const Ref<AssetBaseMesh>& GetCollisionMeshAsset() const { return m_CollisionMeshAsset; }

		bool IsConvex() const { return bConvex; }
		void SetIsConvex(bool bConvex)
		{
			this->bConvex = bConvex;
			if (m_CollisionMeshAsset)
				SetCollisionMeshAsset(m_CollisionMeshAsset);
		}

		bool IsTwoSided() const { return bTwoSided; }
		void SetIsTwoSided(bool bTwoSided)
		{
			this->bTwoSided = bTwoSided;
			if (!bConvex && m_CollisionMeshAsset)
				SetCollisionMeshAsset(m_CollisionMeshAsset);
		}

		virtual void OnRemoved() override;

		const Ref<MeshShape>& GetShape() const { return m_Shapes[0]; }

	protected:
		void UpdatePhysicsTransform() override;
		void UpdatePhysicsMaterials() override;
		void CreateObstacle() override;

	protected:
		std::array<Ref<MeshShape>, 2> m_Shapes; // [0] - front side, [1] - backside. If two-sided collision is enabled, backside will be a valid shape
		Ref<AssetBaseMesh> m_CollisionMeshAsset;
		bool bConvex = true;
		bool bTwoSided = false; // Only affects triangle mesh colliders
	};

	class ScriptComponent
	{
	public:
		ScriptComponent() = default;
		ScriptComponent(const std::string& moduleName) : ModuleName(moduleName) {}

	public:
		std::vector<PublicField> PublicFields;
		std::string ModuleName;
	};

	class NativeScriptComponent
	{
	public:
		NativeScriptComponent() = default;

		~NativeScriptComponent()
		{
			Destroy();
		}

		NativeScriptComponent(NativeScriptComponent&& other) noexcept 
		: Instance(std::move(other.Instance)), 
		InitScript(other.InitScript), m_TypeHash(other.m_TypeHash)
		{
			other.Instance = nullptr; 
			other.InitScript = nullptr;
			other.m_TypeHash = 0;
		}

		NativeScriptComponent& operator=(const NativeScriptComponent& other)
		{
			if (this == &other)
				return *this;

			InitScript = other.InitScript;
			m_TypeHash = other.m_TypeHash;
			return *this;
		}

		NativeScriptComponent& operator=(NativeScriptComponent&& other) noexcept 
		{ 
			Instance = std::move(other.Instance);
			InitScript = other.InitScript;
			m_TypeHash = other.m_TypeHash;

			other.Instance = nullptr;
			other.InitScript = nullptr;
			other.m_TypeHash = 0;

			return *this;
		}

		void OnUpdate(Entity entity, Timestep ts);
		void OnEvent(Entity entity, Event& e);
		void Destroy();

		template<typename T>
		void Bind()
		{
			Destroy();
			m_TypeHash = typeid(T).hash_code();
			InitScript = [](NativeScriptComponent* comp) { comp->Instance = MakeScope<T>(); };
		}

		template<typename T>
		bool Is() const
		{
			return m_TypeHash == typeid(T).hash_code();
		}

		template<typename T>
		T* As()
		{
			return Is<T>() ? (T*)Instance.get() : nullptr;
		}

		template<typename T>
		const T* As() const
		{
			return Is<T>() ? (T*)Instance.get() : nullptr;
		}

	protected:

		void InitScriptsIfNeeded(Entity entity)
		{
			if (!Instance && InitScript)
			{
				InitScript(this);
				Instance->Parent = entity;
				Instance->OnCreate();
			}
		}

	protected:
		Scope<ScriptableEntity> Instance = nullptr;
		void(*InitScript)(NativeScriptComponent*) = nullptr;
		size_t m_TypeHash = 0;
	};

	class AudioComponent : public SceneComponent
	{
	public:
		AudioComponent(const Entity& entity) : SceneComponent(entity) { }
		AudioComponent& operator=(const AudioComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);
			Volume = other.Volume;
			Pitch = other.Pitch;
			Pan = other.Pan;
			LoopCount = other.LoopCount;
			FFTSamples = other.FFTSamples;
			FFTType = other.FFTType;
			bLooping = other.bLooping;
			bMuted = other.bMuted;
			bStreaming = other.bStreaming;
			bEnableFFT = other.bEnableFFT;
			MinDistance = other.MinDistance;
			MaxDistance = other.MaxDistance;
			RollOff = other.RollOff;
			bAutoplay = other.bAutoplay;
			bEnableDopplerEffect = other.bEnableDopplerEffect;
			b3D = other.b3D;

			SetAudioAsset(other.m_AudioAsset);

			return *this;
		}

		AudioComponent(const AudioComponent&) = delete;
		AudioComponent(AudioComponent&&) noexcept = default;
		AudioComponent& operator=(AudioComponent&&) noexcept = default;

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			UpdateSoundPositionAndVelocity();
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			UpdateSoundPositionAndVelocity();
		}

		void SetMinDistance(float minDistance) { SetMinMaxDistance(minDistance, MaxDistance); }
		void SetMaxDistance(float maxDistance) { SetMinMaxDistance(MinDistance, maxDistance); }
		void SetMinMaxDistance(float minDistance, float maxDistance)
		{
			MinDistance = minDistance;
			MaxDistance = maxDistance;
			if (b3D && m_Sound)
				Cast<Sound3D>(m_Sound)->SetMinMaxDistance(MinDistance, MaxDistance);
		}
		float GetMinDistance() const { return MinDistance; }
		float GetMaxDistance() const { return MaxDistance; }

		void SetRollOffModel(RollOffModel rollOff)
		{
			RollOff = rollOff;
			if (b3D && m_Sound)
				Cast<Sound3D>(m_Sound)->SetRollOffModel(RollOff);
		}
		RollOffModel GetRollOffModel() const { return RollOff; }

		void SetVolume(float volume)
		{
			Volume = volume;
			if (m_Sound)
				m_Sound->SetVolumeMultiplier(volume);
		}
		float GetVolume() const { return Volume; }

		void SetPitch(float pitch)
		{
			Pitch = std::clamp(pitch, 0.f, 10.f);
			if (m_Sound)
				m_Sound->SetPitch(Pitch);
		}
		float GetPitch() const { return Pitch; }

		void SetPan(float pan)
		{
			Pan = std::clamp(pan, -1.f, 1.f);
			if (m_Sound)
				m_Sound->SetPan(Pan);
		}
		float GetPan() const { return Pan; }

		void SetLoopCount(int loopCount)
		{
			LoopCount = loopCount;
			if (m_Sound)
				m_Sound->SetLoopCount(loopCount);
		}
		int GetLoopCount() const { return LoopCount; }

		void SetFFTEnabled(bool bEnable)
		{
			bEnableFFT = bEnable;
			if (m_Sound)
				m_Sound->SetFFTEnabled(bEnable);
		}
		bool IsFFTEnabled() const { return bEnableFFT; }

		void SetFFTSamples(uint32_t samples)
		{
			if (samples == FFTSamples)
				return;

			uint32_t power = 0u;
			if (samples > FFTSamples)
				power = (uint32_t)std::ceil(std::log2(samples));
			else
				power = (uint32_t)std::floor(std::log2(samples));
			samples = (uint32_t)glm::pow(2u, power);
			samples = glm::clamp(samples, 64u, 8192u);

			FFTSamples = samples;
			if (m_Sound)
				m_Sound->SetFFTSamples(samples);
		}
		uint32_t GetFFTSamples() const { return FFTSamples; }

		void SetFFTType(FFTWindowType type)
		{
			FFTType = type;
			if (m_Sound)
				m_Sound->SetFFTType(FFTType);
		}
		FFTWindowType GetFFTType() const { return FFTType; }

		void SetLooping(bool bLooping)
		{
			this->bLooping = bLooping;
			if (m_Sound)
				m_Sound->SetLooping(bLooping);
		}
		bool IsLooping() const { return bLooping; }

		void SetMuted(bool bMuted)
		{
			this->bMuted = bMuted;
			if (m_Sound)
				m_Sound->SetMuted(bMuted);
		}
		bool IsMuted() const { return bMuted; }

		void SetAudioAsset(const Ref<AssetAudio>& asset)
		{
			if (asset)
			{
				m_AudioAsset = asset;
				if (b3D)
				{
					m_Sound = Create3D();
				}
				else
				{
					m_Sound = Create2D();
				}
			}
			else
			{
				m_AudioAsset = nullptr;
				m_Sound.reset();
			}
		}
		const Ref<AssetAudio>& GetAudioAsset() const { return m_AudioAsset; }
		
		void SetStreaming(bool bStreaming)
		{
			this->bStreaming = bStreaming;
			if (m_Sound)
				m_Sound->SetStreaming(bStreaming);
		}
		bool IsStreaming() const { return bStreaming; }

		void Play()
		{
			if (m_Sound)
				m_Sound->Play();
		}
		void Stop()
		{
			if (m_Sound)
				m_Sound->Stop();
		}
		void SetPaused(bool bPaused)
		{
			if (m_Sound)
				m_Sound->SetPaused(bPaused);
		}
		bool IsPlaying() const 
		{
			if (m_Sound)
				return m_Sound->IsPlaying();
			return false;
		}

		// dataCount. Array size of `outData`
		// channelIndex. Allows to get data from a specific audio channel. Starts from 0. If -1, get average over all channels
		bool GetSpectrumData(float* outData, uint32_t dataCount, int channelIndex = -1) const
		{
			if (m_Sound)
				return m_Sound->GetSpectrumData(outData, dataCount, channelIndex);
			return false;
		}

		float GetSampleRate() const
		{
			if (m_Sound)
				return m_Sound->GetSampleRate();
			
			return 0.f;
		}

		int GetChannelsCount() const
		{
			if (m_Sound)
				return m_Sound->GetChannelsCount();
			
			return 0;
		}

		void SetPosition(uint32_t ms)
		{
			if (m_Sound)
				m_Sound->SetPosition(ms);
		}
		uint32_t GetPosition()
		{
			return m_Sound ? m_Sound->GetPosition() : 0u;
		}

		void SetIs3D(bool bValue)
		{
			if (bValue == b3D)
				return;

			b3D = bValue;
			if (m_Sound)
			{
				const bool bPlaying = m_Sound->IsPlaying();
				const bool bPaused = m_Sound->IsPaused();
				uint32_t ms = m_Sound->GetPosition();

				if (b3D)
					m_Sound = Create3D();
				else
					m_Sound = Create2D();

				if (bPlaying)
				{
					m_Sound->Play();
				}
				else if (bPaused)
				{
					m_Sound->Play();
					m_Sound->SetPaused(true);
				}

				if (m_Sound->IsPlaying())
					m_Sound->SetPosition(ms);
			}
		}
		bool Is3D() const { return b3D; }

	private:
		void UpdateSoundPositionAndVelocity()
		{
			if (b3D && m_Sound)
			{
				if (bEnableDopplerEffect)
					Cast<Sound3D>(m_Sound)->SetWorldPositionAndVelocity(WorldTransform.Location, Parent.GetLinearVelocity());
				else
					Cast<Sound3D>(m_Sound)->SetWorldPositionAndVelocity(WorldTransform.Location, glm::vec3{ 0.f });
			}
		}

		Ref<Sound2D> Create2D()
		{
			return Sound2D::Create(m_AudioAsset->GetAudio(), GetSoundSettings());
		}

		Ref<Sound3D> Create3D()
		{
			Ref<Sound3D> sound = Sound3D::Create(m_AudioAsset->GetAudio(), WorldTransform.Location, RollOff, GetSoundSettings());
			sound->SetMinMaxDistance(MinDistance, MaxDistance);
			return sound;
		}

		SoundSettings GetSoundSettings() const
		{
			SoundSettings settings;
			settings.VolumeMultiplier = Volume;
			settings.Pan = Pan;
			settings.Pitch = Pitch;
			settings.LoopCount = LoopCount;
			settings.FFTSamples = FFTSamples;
			settings.FFTType = FFTType;
			settings.IsLooping = bLooping;
			settings.IsMuted = bMuted;
			settings.IsStreaming = bStreaming;
			settings.bEnableFFT = bEnableFFT;

			return settings;
		}
	
	protected:
		Ref<AssetAudio> m_AudioAsset;
		Ref<Sound> m_Sound;
		float Volume = 1.f;
		float Pitch = 1.f;
		float Pan = 0.f;
		int LoopCount = -1;
		uint32_t FFTSamples = 256;
		FFTWindowType FFTType = FFTWindowType::Rect;
		bool bLooping = false;
		bool bMuted = false;
		bool bStreaming = false;
		bool bEnableFFT = false;
		bool b3D = true;
		float MinDistance = 1.f;
		float MaxDistance = 10000.f;
		RollOffModel RollOff = RollOffModel::Default;
	public:
		bool bAutoplay = true;
		bool bEnableDopplerEffect = false;
	};

	class ReverbComponent : public SceneComponent
	{
	public:
		ReverbComponent(const Entity& entity) : SceneComponent(entity) { OnInit(); }
		ReverbComponent& operator=(const ReverbComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);
			m_bVisualize = other.m_bVisualize;
			if (other.m_Reverb)
				m_Reverb = Reverb3D::Create(other.m_Reverb);
			if (m_bVisualize)
				Parent.SignalComponentChanged<ReverbComponent>(Notification::OnDebugStateChanged);

			return *this;
		}

		ReverbComponent(const ReverbComponent&) = delete;
		ReverbComponent(ReverbComponent&&) noexcept = default;
		ReverbComponent& operator=(ReverbComponent&&) noexcept = default;

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			if (m_Reverb)
				m_Reverb->SetPosition(WorldTransform.Location);
			if (m_bVisualize)
				Parent.SignalComponentChanged<ReverbComponent>(Notification::OnDebugStateChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			if (m_Reverb)
				m_Reverb->SetPosition(WorldTransform.Location);
			if (m_bVisualize)
				Parent.SignalComponentChanged<ReverbComponent>(Notification::OnDebugStateChanged);
		}

		void SetPreset(ReverbPreset preset) { m_Reverb->SetPreset(preset); }
		void SetActive(bool bActive) { return m_Reverb->SetActive(bActive); }
		bool IsActive() const { return m_Reverb->IsActive();; }

		void SetMinDistance(float minDistance)
		{
			m_Reverb->SetMinDistance(minDistance);
			if (m_bVisualize)
				Parent.SignalComponentChanged<ReverbComponent>(Notification::OnDebugStateChanged);
		}

		void SetMaxDistance(float maxDistance)
		{
			m_Reverb->SetMaxDistance(maxDistance);
			
			if (m_bVisualize)
				Parent.SignalComponentChanged<ReverbComponent>(Notification::OnDebugStateChanged);
		}

		void SetMinMaxDistance(float minDistance, float maxDistance)
		{
			m_Reverb->SetMinMaxDistance(minDistance, maxDistance);
			if (m_bVisualize)
				Parent.SignalComponentChanged<ReverbComponent>(Notification::OnDebugStateChanged);
		}

		float GetMinDistance() const { return m_Reverb->GetMinDistance(); }
		float GetMaxDistance() const { return m_Reverb->GetMaxDistance(); }
		ReverbPreset GetPreset() const { return m_Reverb->GetPreset(); }

		const Ref<Reverb3D>& GetReverb() const { return m_Reverb; }

		void SetVisualizeRadiusEnabled(bool bEnable)
		{
			m_bVisualize = bEnable;
			Parent.SignalComponentChanged<ReverbComponent>(Notification::OnDebugStateChanged);
		}
		bool IsVisualizeRadiusEnabled() const { return m_bVisualize; }

	private:
		void OnInit()
		{
			m_Reverb->SetPosition(WorldTransform.Location);
		}

	private:
		Ref<Reverb3D> m_Reverb = Reverb3D::Create();
		bool m_bVisualize = false;
	};

	class ParticleSystemComponent : public SceneComponent
	{
	public:
		// Note: Scene is responsible for ParticleSystem creation/destruction when this component is being created/deleted
		ParticleSystemComponent(const Entity& entity) : SceneComponent(entity) { }
		~ParticleSystemComponent();

		ParticleSystemComponent& operator=(const ParticleSystemComponent& other);
		ParticleSystemComponent(const ParticleSystemComponent&) = delete;
		ParticleSystemComponent(ParticleSystemComponent&&) noexcept = default;
		ParticleSystemComponent& operator=(ParticleSystemComponent&&) noexcept = default;

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<ParticleSystemComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<ParticleSystemComponent>(Notification::OnTransformChanged);
		}

		void SetAsset(const Ref<AssetParticleSystem>& asset);

		const Ref<AssetParticleSystem>& GetAsset() const { return m_Asset; }

		void Spawn();
		void Destroy();
		void Update();

		const GUID& GetSystemID() const { return m_SystemID; }

		struct AnimData
		{
			SkeletalPose LastPose; // The final pose that was calculated during the last animation update
			float CurrentClipPlayTime = 0.f;
			float PrevClipPlayTime = 0.f;
			float PrevClipPlaybackSpeed = 1.f;
			Entity SrcOfLastPose = Entity::Null; // If valid, its skeletal mesh component last pose will be used instead
		};
		std::vector<AnimData> PerEmitterAnimData;

	private:
		void UpdatePerEmitterAnimData();

	private:
		Ref<AssetParticleSystem> m_Asset;
		GUID m_SystemID = {};
		bool bSpawned = false;

	public:
		bool bAutospawn = true;
	};

	class DecalComponent : public SceneComponent
	{
	public:
		DecalComponent(const Entity& entity) : SceneComponent(entity) {}
		COMPONENT_DEFAULTS(DecalComponent);

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<DecalComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<DecalComponent>(Notification::OnTransformChanged);
		}

		void SetMaterialAsset(const Ref<AssetMaterial>& material)
		{
			m_MaterialAsset = material;
			Parent.SignalComponentChanged<DecalComponent>(Notification::OnStateChanged);
		}

		void SetSortPriority(uint32_t sortPriority)
		{
			m_SortPriority = sortPriority;
			Parent.SignalComponentChanged<DecalComponent>(Notification::OnStateChanged);
		}

		void SetAdjustAspectRatioEnabled(bool bAdjustAspectRatio)
		{
			m_AdjustAspectRatio = bAdjustAspectRatio;
			Parent.SignalComponentChanged<DecalComponent>(Notification::OnStateChanged);
		}
		
		void SetVisible(bool bVisible)
		{
			m_bVisible = bVisible;
			Parent.SignalComponentChanged<DecalComponent>(Notification::OnStateChanged);
		}
		bool IsVisible() const { return m_bVisible; }

		const Ref<AssetMaterial>& GetMaterialAsset() const { return m_MaterialAsset; }
		uint32_t GetSortPriority() const { return m_SortPriority; }
		bool IsAdjustAspectRatioEnabled() const { return m_AdjustAspectRatio; }

	private:
		Ref<AssetMaterial> m_MaterialAsset;
		uint32_t m_SortPriority = 0u;
		bool m_AdjustAspectRatio = true;
		bool m_bVisible = true;
	};

	class NavigationMeshComponent : public SceneComponent
	{
	public:
		NavigationMeshComponent(const Entity& entity) : SceneComponent(entity) {}

		NavigationMeshComponent& operator=(const NavigationMeshComponent& other);
		NavigationMeshComponent(const NavigationMeshComponent&) = delete;
		NavigationMeshComponent(NavigationMeshComponent&&) noexcept = default;
		NavigationMeshComponent& operator=(NavigationMeshComponent&&) noexcept = default;

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			OnChanged();
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			OnChanged();
		}

		void SetSettings(const AINavigation::MeshSettings& settings)
		{
			m_Settings = settings;
			OnChanged();
		}

		void SetCrowdSettings(const AINavigation::CrowdSettings& settings)
		{
			m_CrowdSettings = settings;
			Parent.GetScene()->BuildCrowd(m_CrowdSettings);
		}

		const AINavigation::MeshSettings& GetSettings() const { return m_Settings; }
		const AINavigation::CrowdSettings& GetCrowdSettings() const { return m_CrowdSettings; }

		void GetNavMeshDebugDraw(duDebugDraw* debugDraw) const;
		const Ref<AINavigation::Mesh>& GetNavMesh() const { return m_NavMesh; }

		void Update(Timestep ts);

	public:
		bool bAutoRebuild = false; // Rebuilds on changes if activated

	private:
		void OnChanged()
		{
			Parent.SignalComponentChanged<NavigationMeshComponent>(Notification::OnStateChanged);
		}

	private:
		AINavigation::MeshSettings m_Settings;
		AINavigation::CrowdSettings m_CrowdSettings;
		Ref<AINavigation::Mesh> m_NavMesh;

		// TODO: Ugly, but we only support one NavMesh, so for it to work, scene must control it
		friend class Scene;
		void Build();
		void DestroyNavMesh() { m_NavMesh.reset(); }
	};

	class NavigationCrowdAgentComponent : public Component
	{
	public:
		NavigationCrowdAgentComponent(const Entity& entity) : Component(entity) {}

		NavigationCrowdAgentComponent& operator=(const NavigationCrowdAgentComponent& other);
		NavigationCrowdAgentComponent(const NavigationCrowdAgentComponent&) = delete;
		NavigationCrowdAgentComponent(NavigationCrowdAgentComponent&&) noexcept = default;
		NavigationCrowdAgentComponent& operator=(NavigationCrowdAgentComponent&&) noexcept = default;

		// Agents are controlled by the crowd system. But if teleportation is required,
		// this function can be used. It'll recreate an agent at a new location
		void TeleportAgent(const glm::vec3& location);

		void SetMoveTarget(const glm::vec3& pos);
		void ResetMoveTarget();

		bool GetLocation(glm::vec3* outLocation) const;
		bool GetVelocity(glm::vec3* outVelocity) const;
		MoveRequestState GetAgentTargetState() const;

		const AINavigation::AgentSettings& GetSettings() const { return m_Settings; }
		void SetSettings(const AINavigation::AgentSettings& settings);

		bool IsValid() const { return m_AgentIndex != -1; }

	private:
		// TODO: Ugly, but we only support one NavMesh, so for it to work, scene must control it
		void CreateAgent(const glm::vec3& location);
		void RemoveAgent();
		friend class Scene;

	private:
		AINavigation::AgentSettings m_Settings{};
		int m_AgentIndex = -1;
	};
}
