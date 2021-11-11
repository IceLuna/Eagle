#pragma once

#include "SceneComponent.h"

#include "Eagle/Core/ScriptableEntity.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Camera/SceneCamera.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/SubTexture2D.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Script/PublicField.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsMaterial.h"
#include "Eagle/Audio/Sound3D.h"
#include "Eagle/Audio/Reverb3D.h"

// If new component class is created, you need to make other changes too:
// 1) Add new line into Scene's copy constructor;
// 2) Add new line into Scene::CreateFromEntity function;
// 3) Make it serializable;
// 4) Add it to SceneHierarchyPanel to draw UI
// 5) Add to ScriptEngineRegistry

namespace Eagle
{
	class IDComponent : public Component
	{
	public:
		IDComponent() = default;
		IDComponent(const GUID& other) : Component(), ID(other) {}
		COMPONENT_DEFAULTS(IDComponent);

		GUID ID;
	};

	class OwnershipComponent : public Component
	{
	public:
		OwnershipComponent() = default;

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

	class EntitySceneNameComponent : public Component
	{
	public:
		EntitySceneNameComponent() = default;
		COMPONENT_DEFAULTS(EntitySceneNameComponent);
		EntitySceneNameComponent(const std::string& name) : Component(name) {}
	};

	class TransformComponent : public Component
	{
	public:
		TransformComponent() = default;
		COMPONENT_DEFAULTS(TransformComponent);
		Transform WorldTransform;
		Transform RelativeTransform;
	};

	class LightComponent : public SceneComponent
	{
	public:
		LightComponent() = default;
		LightComponent(const glm::vec3 lightColor) : LightColor(lightColor) {}
		COMPONENT_DEFAULTS(LightComponent);
		
	public:
		glm::vec3 LightColor = glm::vec3(1.f);
		glm::vec3 Ambient = glm::vec3(0.2f);
		glm::vec3 Specular = glm::vec3(0.5f);
		bool bAffectsWorld = true;
	};

	class PointLightComponent : public LightComponent
	{
	public:
		PointLightComponent() = default;
		COMPONENT_DEFAULTS(PointLightComponent);
		float Intensity = 1.f;
	};

	class DirectionalLightComponent : public LightComponent
	{};

	class SpotLightComponent : public LightComponent
	{
	public:
		SpotLightComponent() = default;
		COMPONENT_DEFAULTS(SpotLightComponent);
		float InnerCutOffAngle = 25.f;
		float OuterCutOffAngle = 45.f;
		float Intensity = 1.f;
	};

	class SpriteComponent : public SceneComponent
	{
	public:
		SpriteComponent() : Material(Material::Create()) { Material->Shader = ShaderLibrary::GetOrLoad("assets/shaders/SpriteShader.glsl"); }
		SpriteComponent(const SpriteComponent&) = delete;
		SpriteComponent(SpriteComponent&&) noexcept = default;

		SpriteComponent& operator=(const SpriteComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);

			Material = Material::Create(other.Material);
			SubTexture = other.SubTexture;
			SubTextureCoords = other.SubTextureCoords;
			SpriteSize = other.SpriteSize;
			SpriteSizeCoef = other.SpriteSizeCoef;
			bSubTexture = other.bSubTexture;

			return *this;
		}

		SpriteComponent& operator=(SpriteComponent&&) noexcept = default;

		Ref<Eagle::Material> Material;
		Ref<SubTexture2D> SubTexture;
		glm::vec2 SubTextureCoords = {0, 0};
		glm::vec2 SpriteSize = {64, 64};
		glm::vec2 SpriteSizeCoef = {1, 1};
		bool bSubTexture = false;
	};

	class StaticMeshComponent : public SceneComponent
	{
	public:
		StaticMeshComponent() = default;
		StaticMeshComponent(const StaticMeshComponent&) = delete;
		StaticMeshComponent(StaticMeshComponent&&) noexcept = default;

		StaticMeshComponent& operator=(const StaticMeshComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);
			if (other.StaticMesh)
				StaticMesh = StaticMesh::Create(other.StaticMesh);

			return *this;
		}

		StaticMeshComponent& operator=(StaticMeshComponent&&) noexcept = default;

	public:
		Ref<Eagle::StaticMesh> StaticMesh;
	};

	class CameraComponent : public SceneComponent
	{
	public:
		CameraComponent() = default;
		COMPONENT_DEFAULTS(CameraComponent);

		glm::mat4 GetViewProjection() const
		{
			glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), WorldTransform.Location);
			transformMatrix *= Math::GetRotationMatrix(WorldTransform.Rotation);

			glm::mat4 ViewMatrix = glm::inverse(transformMatrix);
			glm::mat4 ViewProjection = Camera.GetProjection() * ViewMatrix;

			return ViewProjection;
		}
		
		glm::mat4 GetViewMatrix() const
		{
			glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), WorldTransform.Location);
			transformMatrix *= WorldTransform.Rotation.ToMat4();

			glm::mat4 ViewMatrix = glm::inverse(transformMatrix);
			return ViewMatrix;
		}

	public:
		SceneCamera Camera;
		bool Primary = false; //TODO: think about moving to Scene
		bool FixedAspectRatio = false;
	};

	class RigidBodyComponent : public SceneComponent
	{
	public:
		RigidBodyComponent() = default;
		COMPONENT_DEFAULTS(RigidBodyComponent);

	public:
		enum class Type { Static, Dynamic };
		enum class CollisionDetectionType { Discrete, Continuous, ContinuousSpeculative };
		Type BodyType = Type::Static;
		CollisionDetectionType CollisionDetection = CollisionDetectionType::Discrete;
		float Mass = 1.f;
		float LinearDamping = 0.01f;
		float AngularDamping = 0.05f;
		bool EnableGravity = false;
		bool IsKinematic = false;
		
		bool LockPositionX = false;
		bool LockPositionY = false;
		bool LockPositionZ = false;
		bool LockRotationX = false;
		bool LockRotationY = false;
		bool LockRotationZ = false;
	};

	class BaseColliderComponent : public SceneComponent
	{
	protected:
		BaseColliderComponent() = default;
		COMPONENT_DEFAULTS(BaseColliderComponent);

	public:
		Ref<PhysicsMaterial> Material = MakeRef<PhysicsMaterial>(0.6f, 0.6f, 0.5f);
		Ref<StaticMesh> DebugMesh;
		bool IsTrigger = false;
	};

	class BoxColliderComponent : public BaseColliderComponent
	{
	public:
		BoxColliderComponent() = default;
		COMPONENT_DEFAULTS(BoxColliderComponent);

		glm::vec3 Size = glm::vec3(1.f);
	};

	class SphereColliderComponent : public BaseColliderComponent
	{
	public:
		SphereColliderComponent() = default;
		COMPONENT_DEFAULTS(SphereColliderComponent);

		float Radius = 0.5f;
	};

	class CapsuleColliderComponent : public BaseColliderComponent
	{
	public:
		CapsuleColliderComponent() = default;
		COMPONENT_DEFAULTS(CapsuleColliderComponent);
		float Radius = 0.5f;
		float Height = 1.f;
	};

	class MeshColliderComponent : public BaseColliderComponent
	{
	public:
		MeshColliderComponent() = default;
		COMPONENT_DEFAULTS(MeshColliderComponent);

		virtual void OnInit(Entity& entity) override
		{
			BaseColliderComponent::OnInit(entity);

			if (Parent)
			{
				if (Parent.HasComponent<StaticMeshComponent>())
					CollisionMesh = Parent.GetComponent<StaticMeshComponent>().StaticMesh;
			}
		}

		Ref<StaticMesh> CollisionMesh;
		bool IsConvex = true;
	};

	class ScriptComponent : public Component
	{
	public:
		ScriptComponent() = default;
		ScriptComponent(const std::string& moduleName) : Component(), ModuleName(moduleName) {}
		COMPONENT_DEFAULTS(ScriptComponent);

	public:
		std::unordered_map<std::string, PublicField> PublicFields;
		std::string ModuleName;
	};

	class NativeScriptComponent : public Component
	{
	public:
		NativeScriptComponent() = default;

		NativeScriptComponent(NativeScriptComponent&& other) noexcept 
		:Instance(other.Instance), 
		InitScript(other.InitScript), 
		DestroyScript(other.DestroyScript) 
		{
			other.Instance = nullptr; 
			other.InitScript = nullptr; 
			other.DestroyScript = nullptr; 
		}

		NativeScriptComponent& operator=(const NativeScriptComponent& other) 
		{ 
			if (this == &other)
				return *this;

			Instance = nullptr;
			InitScript = other.InitScript;
			DestroyScript = other.DestroyScript;

			return *this;
		}

		NativeScriptComponent& operator=(NativeScriptComponent&& other) noexcept 
		{ 
			Instance = other.Instance;
			InitScript = other.InitScript;
			DestroyScript = other.DestroyScript;

			other.Instance = nullptr;
			other.InitScript = nullptr;
			other.DestroyScript = nullptr;

			return *this;
		}

		template<typename T>
		void Bind()
		{
			EG_CORE_ASSERT(InitScript == nullptr, "Initialized Script twice!");
			InitScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}

	protected:
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InitScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		friend class Scene;
		friend void DestroyScript(NativeScriptComponent*);
	};

	class AudioComponent : public SceneComponent
	{
	public:
		AudioComponent() = default;
		AudioComponent& operator=(const AudioComponent& other)
		{
			if (other.Sound)
				Sound = Sound3D::Create(other.Sound->GetSoundPath(), other.GetWorldTransform().Location, other.Sound->GetRollOffModel(), other.Settings);
			Settings = other.Settings;
			MinDistance = other.MinDistance;
			MaxDistance = other.MaxDistance;
			RollOff = other.RollOff;
			bAutoplay = other.bAutoplay;
			bEnableDopplerEffect = other.bEnableDopplerEffect;

			return *this;
		}

		AudioComponent(const AudioComponent&) = delete;
		AudioComponent(AudioComponent&&) noexcept = default;
		AudioComponent& operator=(AudioComponent&&) noexcept = default;

	protected:
		virtual void OnNotify(Notification notification) override
		{
			SceneComponent::OnNotify(notification);
			if (notification == Notification::OnParentTransformChanged)
			{
				if (Sound)
					if (bEnableDopplerEffect)
						Sound->SetPositionAndVelocity(WorldTransform.Location, Parent.GetLinearVelocity());
					else
						Sound->SetPositionAndVelocity(WorldTransform.Location, glm::vec3{0.f});
			}
		}
	
	public:
		Ref<Sound3D> Sound;
		SoundSettings Settings;
		float MinDistance = 1.f;
		float MaxDistance = 10000.f;
		RollOffModel RollOff = RollOffModel::Default;
		bool bAutoplay = true;
		bool bEnableDopplerEffect = false;
	};

	class ReverbComponent : public SceneComponent
	{
	public:
		ReverbComponent() = default;
		ReverbComponent& operator=(const ReverbComponent& other)
		{
			if (other.Reverb)
				Reverb = Reverb3D::Create(other.Reverb);

			return *this;
		}

		ReverbComponent(const ReverbComponent&) = delete;
		ReverbComponent(ReverbComponent&&) noexcept = default;
		ReverbComponent& operator=(ReverbComponent&&) noexcept = default;

		virtual void OnInit(Entity& entity) override
		{
			SceneComponent::OnInit(entity);
			Reverb->SetPosition(WorldTransform.Location);
		}

	protected:
		virtual void OnNotify(Notification notification) override
		{
			SceneComponent::OnNotify(notification);
			if (notification == Notification::OnParentTransformChanged)
			{
				if (Reverb)
					Reverb->SetPosition(WorldTransform.Location);
			}
		}

	public:
		Ref<Reverb3D> Reverb = Reverb3D::Create();
	};
}