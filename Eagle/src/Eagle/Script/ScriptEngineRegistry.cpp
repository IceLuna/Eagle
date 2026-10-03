#include "egpch.h"
#include "ScriptEngineRegistry.h"
#include "ScriptWrappers.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/Entity.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

namespace Eagle
{
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&)>> m_RemoveComponentFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;

	//SceneComponents
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetWorldTransformFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetRelativeTransformFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetWorldTransformFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetRelativeTransformFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetForwardVectorFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetRightVectorFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetUpVectorFunctions;

	//Light Component
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetLightColorFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetLightColorFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsWorldFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_GetAffectsWorldFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<float(Entity&)>> m_GetIntensityFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, float)>> m_SetIntensityFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<float(Entity&)>> m_GetVolumetricFogIntensityFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, float)>> m_SetVolumetricFogIntensityFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetCastsShadowsFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_GetCastsShadowsFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsVolumetricLightFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_GetIsVolumetricLightFunctions;

	//BaseColliderComponent
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, CollisionGroup)>> m_SetCollisionGroupsFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<CollisionGroup(Entity&)>> m_GetCollisionGroupsFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, CollisionGroup)>> m_SetInteractingCollisionGroupsFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<CollisionGroup(Entity&)>> m_GetInteractingCollisionGroupsFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsTriggerFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsTriggerFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetCollisionEnabledFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsCollisionEnabledFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetCollisionVisibleFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsCollisionVisibleFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const Ref<AssetPhysicsMaterial>&)>> m_SetPhysicsMaterialFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<GUID(Entity&)>> m_GetPhysicsMaterialFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsNavMeshBuildFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_DoesAffectNavMeshBuildFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsObstacleFunctions;
	ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsObstacleFunctions;

	// Scene
	ankerl::unordered_dense::map<MonoType*, std::function<std::vector<Entity>(const Ref<Scene>&)>> m_GetAllEntitiesWith;

	extern MonoImage* s_CoreAssemblyImage;

#define REGISTER_COMPONENT_TYPE(Type)\
	{\
		MonoType* type = mono_reflection_type_from_name((char*)("Eagle." #Type), s_CoreAssemblyImage);\
		if (type)\
		{\
			m_HasComponentFunctions[type] = [](Entity& entity) { return entity.HasComponent<Type>(); };\
			m_AddComponentFunctions[type] = [](Entity& entity) { entity.AddComponent<Type>(); };\
			m_RemoveComponentFunctions[type] = [](Entity& entity) { entity.RemoveComponent<Type>(); };\
			m_GetAllEntitiesWith[type] = [](const Ref<Scene>& scene) { return scene->GetAllEntitiesWith_Vector<Type>(); };\
			\
			if constexpr (std::is_base_of<SceneComponent, Type>::value)\
			{\
				m_SetWorldTransformFunctions[type] = [](Entity& entity, const Transform* transform) { ((SceneComponent&)entity.GetComponent<Type>()).SetWorldTransform(*transform); };\
				m_SetRelativeTransformFunctions[type] = [](Entity& entity, const Transform* transform) { ((SceneComponent&)entity.GetComponent<Type>()).SetRelativeTransform(*transform); };\
				\
				m_GetWorldTransformFunctions[type] = [](Entity& entity, Transform* transform) { *transform = ((SceneComponent&)entity.GetComponent<Type>()).GetWorldTransform(); };\
				m_GetRelativeTransformFunctions[type] = [](Entity& entity, Transform* transform) { *transform = ((SceneComponent&)entity.GetComponent<Type>()).GetRelativeTransform(); };\
				m_GetForwardVectorFunctions[type] = [](Entity& entity, glm::vec3* outVector) { *outVector = ((SceneComponent&)entity.GetComponent<Type>()).GetForwardVector(); };\
				m_GetRightVectorFunctions[type] = [](Entity& entity, glm::vec3* outVector) { *outVector = ((SceneComponent&)entity.GetComponent<Type>()).GetRightVector(); };\
				m_GetUpVectorFunctions[type] = [](Entity& entity, glm::vec3* outVector) { *outVector = ((SceneComponent&)entity.GetComponent<Type>()).GetUpVector(); };\
			}\
			\
			if constexpr (std::is_base_of<LightComponent, Type>::value)\
			{\
				m_SetLightColorFunctions[type] = [](Entity& entity, const glm::vec3* value) { ((LightComponent&)entity.GetComponent<Type>()).SetLightColor(*value); };\
				m_GetLightColorFunctions[type] = [](Entity& entity, glm::vec3* outValue) { *outValue = ((LightComponent&)entity.GetComponent<Type>()).GetLightColor(); };\
				\
				m_SetAffectsWorldFunctions[type] = [](Entity& entity, bool value) { ((LightComponent&)entity.GetComponent<Type>()).SetAffectsWorld(value); };\
				m_GetAffectsWorldFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).DoesAffectWorld(); };\
				\
				m_GetIntensityFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).GetIntensity(); };\
				m_SetIntensityFunctions[type] = [](Entity& entity, float value) { return ((LightComponent&)entity.GetComponent<Type>()).SetIntensity(value); };\
				\
				m_GetVolumetricFogIntensityFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).GetVolumetricFogIntensity(); };\
				m_SetVolumetricFogIntensityFunctions[type] = [](Entity& entity, float value) { return ((LightComponent&)entity.GetComponent<Type>()).SetVolumetricFogIntensity(value); };\
				\
				m_SetCastsShadowsFunctions[type] = [](Entity& entity, bool value) { ((LightComponent&)entity.GetComponent<Type>()).SetCastsShadows(value); };\
				m_GetCastsShadowsFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).DoesCastShadows(); };\
				\
				m_SetIsVolumetricLightFunctions[type] = [](Entity& entity, bool value) { ((LightComponent&)entity.GetComponent<Type>()).SetIsVolumetricLight(value); };\
				m_GetIsVolumetricLightFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).IsVolumetricLight(); };\
				\
			}\
			if constexpr (std::is_base_of<BaseColliderComponent, Type>::value)\
			{\
				m_SetCollisionGroupsFunctions[type] = [](Entity& entity, CollisionGroup groups) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetCollisionGroup(groups); };\
				m_GetCollisionGroupsFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).GetCollisionGroup(); };\
				m_SetInteractingCollisionGroupsFunctions[type] = [](Entity& entity, CollisionGroup groups) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetInteractingCollisionGroup(groups); };\
				m_GetInteractingCollisionGroupsFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).GetInteractingCollisionGroup(); };\
				m_SetIsTriggerFunctions[type] = [](Entity& entity, bool bTrigger) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetIsTrigger(bTrigger); };\
				m_SetCollisionEnabledFunctions[type] = [](Entity& entity, bool bEnabled) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetCollisionEnabled(bEnabled); };\
				m_IsTriggerFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).IsTrigger(); };\
				m_IsCollisionEnabledFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).IsCollisionEnabled(); };\
				m_SetCollisionVisibleFunctions[type] = [](Entity& entity, bool bVisible) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetShowCollision(bVisible); };\
				m_IsCollisionVisibleFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).IsCollisionVisible(); };\
				m_SetPhysicsMaterialFunctions[type] = [](Entity& entity, const Ref<AssetPhysicsMaterial>& asset) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetPhysicsMaterialAsset(asset); };\
				m_GetPhysicsMaterialFunctions[type] = [](Entity& entity) { const auto& asset = ((BaseColliderComponent&)entity.GetComponent<Type>()).GetPhysicsMaterialAsset(); return asset ? asset->GetGUID() : GUID(0, 0); };\
				m_SetAffectsNavMeshBuildFunctions[type] = [](Entity& entity, bool bAffects) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetAffectsNavMeshBuild(bAffects); };\
				m_DoesAffectNavMeshBuildFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).DoesAffectNavMeshBuild(); };\
				m_SetIsObstacleFunctions[type] = [](Entity& entity, bool bObstacle) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetIsObstacle(bObstacle); };\
				m_IsObstacleFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).IsObstacle(); };\
			}\
		}\
		else\
			EG_CORE_ERROR("No C# Component found for " #Type "!");\
	}

	static void InitComponentTypes()
	{
		REGISTER_COMPONENT_TYPE(SceneComponent);
		REGISTER_COMPONENT_TYPE(PointLightComponent);
		REGISTER_COMPONENT_TYPE(DirectionalLightComponent);
		REGISTER_COMPONENT_TYPE(SpotLightComponent);
		REGISTER_COMPONENT_TYPE(StaticMeshComponent);
		REGISTER_COMPONENT_TYPE(SkeletalMeshComponent);
		REGISTER_COMPONENT_TYPE(AudioComponent);
		REGISTER_COMPONENT_TYPE(CharacterControllerComponent);
		REGISTER_COMPONENT_TYPE(RigidBodyComponent);
		REGISTER_COMPONENT_TYPE(BoxColliderComponent);
		REGISTER_COMPONENT_TYPE(SphereColliderComponent);
		REGISTER_COMPONENT_TYPE(CapsuleColliderComponent);
		REGISTER_COMPONENT_TYPE(MeshColliderComponent);
		REGISTER_COMPONENT_TYPE(CameraComponent);
		REGISTER_COMPONENT_TYPE(ReverbComponent);
		REGISTER_COMPONENT_TYPE(TextComponent);
		REGISTER_COMPONENT_TYPE(BillboardComponent);
		REGISTER_COMPONENT_TYPE(SpriteComponent);
		REGISTER_COMPONENT_TYPE(ScriptComponent);
		REGISTER_COMPONENT_TYPE(Text2DComponent);
		REGISTER_COMPONENT_TYPE(Image2DComponent);
		REGISTER_COMPONENT_TYPE(ParticleSystemComponent);
		REGISTER_COMPONENT_TYPE(SceneSequenceComponent);
		REGISTER_COMPONENT_TYPE(DecalComponent);
		REGISTER_COMPONENT_TYPE(NavigationMeshComponent);
		REGISTER_COMPONENT_TYPE(NavigationCrowdAgentComponent);
	}

	void ScriptEngineRegistry::RegisterAll()
	{
		InitComponentTypes();
		BindFunctions();
	}
}
