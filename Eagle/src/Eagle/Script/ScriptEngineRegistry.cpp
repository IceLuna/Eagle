#include "egpch.h"
#include "ScriptEngineRegistry.h"
#include "ScriptEngine.h"
#include "ScriptWrappers.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/Entity.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

namespace Eagle
{
	std::unordered_map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&)>> m_RemoveComponentFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;

	//SceneComponents
	std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetWorldTransformFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetRelativeTransformFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetWorldTransformFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetRelativeTransformFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetForwardVectorFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetRightVectorFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetUpVectorFunctions;

	//Light Component
	std::unordered_map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetLightColorFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetLightColorFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsWorldFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetAffectsWorldFunctions;
	std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetIntensityFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetIntensityFunctions;
	std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetVolumetricFogIntensityFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetVolumetricFogIntensityFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetCastsShadowsFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetCastsShadowsFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsVolumetricLightFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetIsVolumetricLightFunctions;

	//BaseColliderComponent
	std::unordered_map<MonoType*, std::function<void(Entity&, CollisionGroup)>> m_SetCollisionGroupsFunctions;
	std::unordered_map<MonoType*, std::function<CollisionGroup(Entity&)>> m_GetCollisionGroupsFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, CollisionGroup)>> m_SetInteractingCollisionGroupsFunctions;
	std::unordered_map<MonoType*, std::function<CollisionGroup(Entity&)>> m_GetInteractingCollisionGroupsFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsTriggerFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsTriggerFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetCollisionEnabledFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsCollisionEnabledFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetCollisionVisibleFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsCollisionVisibleFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, const Ref<AssetPhysicsMaterial>&)>> m_SetPhysicsMaterialFunctions;
	std::unordered_map<MonoType*, std::function<GUID(Entity&)>> m_GetPhysicsMaterialFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsNavMeshBuildFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_DoesAffectNavMeshBuildFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsObstacleFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsObstacleFunctions;

	// Scene
	std::unordered_map<MonoType*, std::function<std::vector<Entity>(const Ref<Scene>&)>> m_GetAllEntitiesWith;

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
		REGISTER_COMPONENT_TYPE(DecalComponent);
		REGISTER_COMPONENT_TYPE(NavigationMeshComponent);
		REGISTER_COMPONENT_TYPE(NavigationCrowdAgentComponent);
	}

	void ScriptEngineRegistry::RegisterAll()
	{
		InitComponentTypes();

		//Entity
		mono_add_internal_call("Eagle.Entity::GetParent_Native", Eagle::Script::Eagle_Entity_GetParent);
		mono_add_internal_call("Eagle.Entity::SetParent_Native", Eagle::Script::Eagle_Entity_SetParent);
		mono_add_internal_call("Eagle.Entity::GetChildren_Native", Eagle::Script::Eagle_Entity_GetChildren);
		mono_add_internal_call("Eagle.Entity::DestroyEntity_Native", Eagle::Script::Eagle_Entity_DestroyEntity);
		mono_add_internal_call("Eagle.Entity::AddComponent_Native", Eagle::Script::Eagle_Entity_AddComponent);
		mono_add_internal_call("Eagle.Entity::RemoveComponent_Native", Eagle::Script::Eagle_Entity_RemoveComponent);
		mono_add_internal_call("Eagle.Entity::HasComponent_Native", Eagle::Script::Eagle_Entity_HasComponent);
		mono_add_internal_call("Eagle.Entity::IsValid_Native", Eagle::Script::Eagle_Entity_IsValid);
		mono_add_internal_call("Eagle.Entity::GetEntityName_Native", Eagle::Script::Eagle_Entity_GetEntityName);
		mono_add_internal_call("Eagle.Entity::GetForwardVector_Native", Eagle::Script::Eagle_Entity_GetForwardVector);
		mono_add_internal_call("Eagle.Entity::GetRightVector_Native", Eagle::Script::Eagle_Entity_GetRightVector);
		mono_add_internal_call("Eagle.Entity::GetUpVector_Native", Eagle::Script::Eagle_Entity_GetUpVector);
		mono_add_internal_call("Eagle.Entity::GetChildrenByName_Native", Eagle::Script::Eagle_Entity_GetChildrenByName);
		mono_add_internal_call("Eagle.Entity::IsMouseHovered_Native", Eagle::Script::Eagle_Entity_IsMouseHovered);
		mono_add_internal_call("Eagle.Entity::IsMouseHoveredByCoord_Native", Eagle::Script::Eagle_Entity_IsMouseHoveredByCoord);
		mono_add_internal_call("Eagle.Entity::SetTag_Native", Eagle::Script::Eagle_Entity_SetTag);
		mono_add_internal_call("Eagle.Entity::GetTag_Native", Eagle::Script::Eagle_Entity_GetTag);

		//Input
		mono_add_internal_call("Eagle.Input::IsMouseButtonPressed_Native", Eagle::Script::Eagle_Input_IsMouseButtonPressed);
		mono_add_internal_call("Eagle.Input::IsKeyPressed_Native", Eagle::Script::Eagle_Input_IsKeyPressed);
		mono_add_internal_call("Eagle.Input::GetMousePosition_Native", Eagle::Script::Eagle_Input_GetMousePosition);
		mono_add_internal_call("Eagle.Input::GetMousePositionInViewport_Native", Eagle::Script::Eagle_Input_GetMousePositionInViewport);
		mono_add_internal_call("Eagle.Input::SetMousePosition_Native", Eagle::Script::Eagle_Input_SetMousePosition);
		mono_add_internal_call("Eagle.Input::SetMousePositionInViewport_Native", Eagle::Script::Eagle_Input_SetMousePositionInViewport);
		mono_add_internal_call("Eagle.Input::SetCursorMode_Native", Eagle::Script::Eagle_Input_SetCursorMode);
		mono_add_internal_call("Eagle.Input::GetCursorMode_Native", Eagle::Script::Eagle_Input_GetCursorMode);

		// Renderer
		mono_add_internal_call("Eagle.Renderer::SetFogSettings_Native", Eagle::Script::Eagle_Renderer_SetFogSettings);
		mono_add_internal_call("Eagle.Renderer::GetFogSettings_Native", Eagle::Script::Eagle_Renderer_GetFogSettings);
		mono_add_internal_call("Eagle.Renderer::SetBloomSettings_Native", Eagle::Script::Eagle_Renderer_SetBloomSettings);
		mono_add_internal_call("Eagle.Renderer::GetBloomSettings_Native", Eagle::Script::Eagle_Renderer_GetBloomSettings);
		mono_add_internal_call("Eagle.Renderer::SetSSAOSettings_Native", Eagle::Script::Eagle_Renderer_SetSSAOSettings);
		mono_add_internal_call("Eagle.Renderer::GetSSAOSettings_Native", Eagle::Script::Eagle_Renderer_GetSSAOSettings);
		mono_add_internal_call("Eagle.Renderer::SetGTAOSettings_Native", Eagle::Script::Eagle_Renderer_SetGTAOSettings);
		mono_add_internal_call("Eagle.Renderer::GetGTAOSettings_Native", Eagle::Script::Eagle_Renderer_GetGTAOSettings);
		mono_add_internal_call("Eagle.Renderer::SetPhotoLinearTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_SetPhotoLinearTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::GetPhotoLinearTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_GetPhotoLinearTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::SetFilmicTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_SetFilmicTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::GetFilmicTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_GetFilmicTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::SetAgXTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_SetAgXTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::GetAgXTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_GetAgXTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::GetGamma_Native", Eagle::Script::Eagle_Renderer_GetGamma);
		mono_add_internal_call("Eagle.Renderer::SetGamma_Native", Eagle::Script::Eagle_Renderer_SetGamma);
		mono_add_internal_call("Eagle.Renderer::GetExposure_Native", Eagle::Script::Eagle_Renderer_GetExposure);
		mono_add_internal_call("Eagle.Renderer::SetExposure_Native", Eagle::Script::Eagle_Renderer_SetExposure);
		mono_add_internal_call("Eagle.Renderer::GetLineWidth_Native", Eagle::Script::Eagle_Renderer_GetLineWidth);
		mono_add_internal_call("Eagle.Renderer::SetLineWidth_Native", Eagle::Script::Eagle_Renderer_SetLineWidth);
		mono_add_internal_call("Eagle.Renderer::SetTonemappingMethod_Native", Eagle::Script::Eagle_Renderer_SetTonemappingMethod);
		mono_add_internal_call("Eagle.Renderer::GetTonemappingMethod_Native", Eagle::Script::Eagle_Renderer_GetTonemappingMethod);
		mono_add_internal_call("Eagle.Renderer::SetAAMethod_Native", Eagle::Script::Eagle_Renderer_SetAAMethod);
		mono_add_internal_call("Eagle.Renderer::GetAAMethod_Native", Eagle::Script::Eagle_Renderer_GetAAMethod);
		mono_add_internal_call("Eagle.Renderer::GetAO_Native", Eagle::Script::Eagle_Renderer_GetAO);
		mono_add_internal_call("Eagle.Renderer::SetAO_Native", Eagle::Script::Eagle_Renderer_SetAO);
		mono_add_internal_call("Eagle.Renderer::SetVSyncEnabled_Native", Eagle::Script::Eagle_Renderer_SetVSyncEnabled);
		mono_add_internal_call("Eagle.Renderer::GetVSyncEnabled_Native", Eagle::Script::Eagle_Renderer_GetVSyncEnabled);
		mono_add_internal_call("Eagle.Renderer::SetSoftShadowsEnabled_Native", Eagle::Script::Eagle_Renderer_SetSoftShadowsEnabled);
		mono_add_internal_call("Eagle.Renderer::GetSoftShadowsEnabled_Native", Eagle::Script::Eagle_Renderer_GetSoftShadowsEnabled);
		mono_add_internal_call("Eagle.Renderer::SetCSMSmoothTransitionEnabled_Native", Eagle::Script::Eagle_Renderer_SetCSMSmoothTransitionEnabled);
		mono_add_internal_call("Eagle.Renderer::GetCSMSmoothTransitionEnabled_Native", Eagle::Script::Eagle_Renderer_GetCSMSmoothTransitionEnabled);
		mono_add_internal_call("Eagle.Renderer::SetVisualizeCascades_Native", Eagle::Script::Eagle_Renderer_SetVisualizeCascades);
		mono_add_internal_call("Eagle.Renderer::GetVisualizeCascades_Native", Eagle::Script::Eagle_Renderer_GetVisualizeCascades);
		mono_add_internal_call("Eagle.Renderer::SetVisualizeLightTiles_Native", Eagle::Script::Eagle_Renderer_SetVisualizeLightTiles);
		mono_add_internal_call("Eagle.Renderer::GetVisualizeLightTiles_Native", Eagle::Script::Eagle_Renderer_GetVisualizeLightTiles);
		mono_add_internal_call("Eagle.Renderer::SetTransparencyLayers_Native", Eagle::Script::Eagle_Renderer_SetTransparencyLayers);
		mono_add_internal_call("Eagle.Renderer::GetTransparencyLayers_Native", Eagle::Script::Eagle_Renderer_GetTransparencyLayers);
		mono_add_internal_call("Eagle.Renderer::GetSkySettings_Native", Eagle::Script::Eagle_Renderer_GetSkySettings);
		mono_add_internal_call("Eagle.Renderer::SetSkySettings_Native", Eagle::Script::Eagle_Renderer_SetSkySettings);
		mono_add_internal_call("Eagle.Renderer::SetUseSkyAsBackground_Native", Eagle::Script::Eagle_Renderer_SetUseSkyAsBackground);
		mono_add_internal_call("Eagle.Renderer::GetUseSkyAsBackground_Native", Eagle::Script::Eagle_Renderer_GetUseSkyAsBackground);
		mono_add_internal_call("Eagle.Renderer::SetVolumetricLightsSettings_Native", Eagle::Script::Eagle_Renderer_SetVolumetricLightsSettings);
		mono_add_internal_call("Eagle.Renderer::GetVolumetricLightsSettings_Native", Eagle::Script::Eagle_Renderer_GetVolumetricLightsSettings);
		mono_add_internal_call("Eagle.Renderer::GetShadowMapsSettings_Native", Eagle::Script::Eagle_Renderer_GetShadowMapsSettings);
		mono_add_internal_call("Eagle.Renderer::GetDepthOfFieldSettings_Native", Eagle::Script::Eagle_Renderer_GetDepthOfFieldSettings);
		mono_add_internal_call("Eagle.Renderer::GetMotionBlurSettings_Native", Eagle::Script::Eagle_Renderer_GetMotionBlurSettings);
		mono_add_internal_call("Eagle.Renderer::GetAutoExposureSettings_Native", Eagle::Script::Eagle_Renderer_GetAutoExposureSettings);
		mono_add_internal_call("Eagle.Renderer::GetScreenSpaceReflectionsSettings_Native", Eagle::Script::Eagle_Renderer_GetScreenSpaceReflectionsSettings);
		mono_add_internal_call("Eagle.Renderer::GetLensSettings_Native", Eagle::Script::Eagle_Renderer_GetLensSettings);
		mono_add_internal_call("Eagle.Renderer::SetShadowMapsSettings_Native", Eagle::Script::Eagle_Renderer_SetShadowMapsSettings);
		mono_add_internal_call("Eagle.Renderer::SetDepthOfFieldSettings_Native", Eagle::Script::Eagle_Renderer_SetDepthOfFieldSettings);
		mono_add_internal_call("Eagle.Renderer::SetMotionBlurSettings_Native", Eagle::Script::Eagle_Renderer_SetMotionBlurSettings);
		mono_add_internal_call("Eagle.Renderer::SetAutoExposureSettings_Native", Eagle::Script::Eagle_Renderer_SetAutoExposureSettings);
		mono_add_internal_call("Eagle.Renderer::SetScreenSpaceReflectionsSettings_Native", Eagle::Script::Eagle_Renderer_SetScreenSpaceReflectionsSettings);
		mono_add_internal_call("Eagle.Renderer::SetLensSettings_Native", Eagle::Script::Eagle_Renderer_SetLensSettings);
		mono_add_internal_call("Eagle.Renderer::SetDepthPrepassEnabled_Native", Eagle::Script::Eagle_Renderer_SetDepthPrepassEnabled);
		mono_add_internal_call("Eagle.Renderer::GetDepthPrepassEnabled_Native", Eagle::Script::Eagle_Renderer_GetDepthPrepassEnabled);
		mono_add_internal_call("Eagle.Renderer::SetTranslucentShadowsEnabled_Native", Eagle::Script::Eagle_Renderer_SetTranslucentShadowsEnabled);
		mono_add_internal_call("Eagle.Renderer::GetTranslucentShadowsEnabled_Native", Eagle::Script::Eagle_Renderer_GetTranslucentShadowsEnabled);
		mono_add_internal_call("Eagle.Renderer::GetCameraTransform_Native", Eagle::Script::Eagle_Renderer_GetCameraTransform);
		mono_add_internal_call("Eagle.Renderer::GetViewportSize_Native", Eagle::Script::Eagle_Renderer_GetViewportSize);
		mono_add_internal_call("Eagle.Renderer::SetRenderSkyboxEnabled_Native", Eagle::Script::Eagle_Renderer_SetRenderSkyboxEnabled);
		mono_add_internal_call("Eagle.Renderer::IsRenderSkyboxEnabled_Native", Eagle::Script::Eagle_Renderer_IsRenderSkyboxEnabled);
		mono_add_internal_call("Eagle.Renderer::SetSkyboxEnabled_Native", Eagle::Script::Eagle_Renderer_SetSkyboxEnabled);
		mono_add_internal_call("Eagle.Renderer::IsSkyboxEnabled_Native", Eagle::Script::Eagle_Renderer_IsSkyboxEnabled);
		mono_add_internal_call("Eagle.Renderer::SetSkybox_Native", Eagle::Script::Eagle_Renderer_SetSkybox);
		mono_add_internal_call("Eagle.Renderer::GetSkybox_Native", Eagle::Script::Eagle_Renderer_GetSkybox);
		mono_add_internal_call("Eagle.Renderer::SetCubemapIntensity_Native", Eagle::Script::Eagle_Renderer_SetCubemapIntensity);
		mono_add_internal_call("Eagle.Renderer::GetCubemapIntensity_Native", Eagle::Script::Eagle_Renderer_GetCubemapIntensity);
		mono_add_internal_call("Eagle.Renderer::SetObjectPickingEnabled_Native", Eagle::Script::Eagle_Renderer_SetObjectPickingEnabled);
		mono_add_internal_call("Eagle.Renderer::IsObjectPickingEnabled_Native", Eagle::Script::Eagle_Renderer_IsObjectPickingEnabled);
		mono_add_internal_call("Eagle.Renderer::Set2DObjectPickingEnabled_Native", Eagle::Script::Eagle_Renderer_Set2DObjectPickingEnabled);
		mono_add_internal_call("Eagle.Renderer::Is2DObjectPickingEnabled_Native", Eagle::Script::Eagle_Renderer_Is2DObjectPickingEnabled);
		mono_add_internal_call("Eagle.Renderer::SetSortOpaqueParticlesEnabled_Native", Eagle::Script::Eagle_Renderer_SetSortOpaqueParticlesEnabled);
		mono_add_internal_call("Eagle.Renderer::IsSortOpaqueParticlesEnabled_Native", Eagle::Script::Eagle_Renderer_IsSortOpaqueParticlesEnabled);
		mono_add_internal_call("Eagle.Renderer::SetDebugLinesDepthTestEnabled_Native", Eagle::Script::Eagle_Renderer_SetDebugLinesDepthTestEnabled);
		mono_add_internal_call("Eagle.Renderer::IsDebugLinesDepthTestEnabled_Native", Eagle::Script::Eagle_Renderer_IsDebugLinesDepthTestEnabled);
		mono_add_internal_call("Eagle.Renderer::DrawLine_Native", Eagle::Script::Eagle_Renderer_DrawLine);
		mono_add_internal_call("Eagle.Renderer::DrawTriangle_Native", Eagle::Script::Eagle_Renderer_DrawTriangle);
		mono_add_internal_call("Eagle.Renderer::DrawArrow_Native", Eagle::Script::Eagle_Renderer_DrawArrow);
		mono_add_internal_call("Eagle.Renderer::DrawAABB_Native", Eagle::Script::Eagle_Renderer_DrawAABB);
		mono_add_internal_call("Eagle.Renderer::DrawBox_Native", Eagle::Script::Eagle_Renderer_DrawBox);
		mono_add_internal_call("Eagle.Renderer::DrawCone_Native", Eagle::Script::Eagle_Renderer_DrawCone);

		mono_add_internal_call("Eagle.AgXTonemappingSettings::GetDefaultLook_Native", Eagle::Script::Eagle_AgXTonemapping_GetDefaultLook);
		mono_add_internal_call("Eagle.AgXTonemappingSettings::GetGoldenLook_Native", Eagle::Script::Eagle_AgXTonemapping_GetGoldenLook);
		mono_add_internal_call("Eagle.AgXTonemappingSettings::GetPunchyLook_Native", Eagle::Script::Eagle_AgXTonemapping_GetPunchyLook);

		// Log
		mono_add_internal_call("Eagle.Log::Trace", Eagle::Script::Eagle_Log_Trace);
		mono_add_internal_call("Eagle.Log::Info", Eagle::Script::Eagle_Log_Info);
		mono_add_internal_call("Eagle.Log::Warn", Eagle::Script::Eagle_Log_Warn);
		mono_add_internal_call("Eagle.Log::Error", Eagle::Script::Eagle_Log_Error);
		mono_add_internal_call("Eagle.Log::Critical", Eagle::Script::Eagle_Log_Critical);

		// Entity transforms
		mono_add_internal_call("Eagle.Entity::GetWorldTransform_Native", Eagle::Script::Eagle_Entity_GetWorldTransform);
		mono_add_internal_call("Eagle.Entity::GetWorldLocation_Native", Eagle::Script::Eagle_Entity_GetWorldLocation);
		mono_add_internal_call("Eagle.Entity::GetWorldRotation_Native", Eagle::Script::Eagle_Entity_GetWorldRotation);
		mono_add_internal_call("Eagle.Entity::GetWorldScale_Native", Eagle::Script::Eagle_Entity_GetWorldScale);
		mono_add_internal_call("Eagle.Entity::SetWorldTransform_Native", Eagle::Script::Eagle_Entity_SetWorldTransform);
		mono_add_internal_call("Eagle.Entity::SetWorldLocation_Native", Eagle::Script::Eagle_Entity_SetWorldLocation);
		mono_add_internal_call("Eagle.Entity::SetWorldRotation_Native", Eagle::Script::Eagle_Entity_SetWorldRotation);
		mono_add_internal_call("Eagle.Entity::SetWorldScale_Native", Eagle::Script::Eagle_Entity_SetWorldScale);
		mono_add_internal_call("Eagle.Entity::GetRelativeTransform_Native", Eagle::Script::Eagle_Entity_GetRelativeTransform);
		mono_add_internal_call("Eagle.Entity::GetRelativeLocation_Native", Eagle::Script::Eagle_Entity_GetRelativeLocation);
		mono_add_internal_call("Eagle.Entity::GetRelativeRotation_Native", Eagle::Script::Eagle_Entity_GetRelativeRotation);
		mono_add_internal_call("Eagle.Entity::GetRelativeScale_Native", Eagle::Script::Eagle_Entity_GetRelativeScale);
		mono_add_internal_call("Eagle.Entity::SetRelativeTransform_Native", Eagle::Script::Eagle_Entity_SetRelativeTransform);
		mono_add_internal_call("Eagle.Entity::SetRelativeLocation_Native", Eagle::Script::Eagle_Entity_SetRelativeLocation);
		mono_add_internal_call("Eagle.Entity::SetRelativeRotation_Native", Eagle::Script::Eagle_Entity_SetRelativeRotation);
		mono_add_internal_call("Eagle.Entity::SetRelativeScale_Native", Eagle::Script::Eagle_Entity_SetRelativeScale);

		//Scene Component
		mono_add_internal_call("Eagle.SceneComponent::GetWorldTransform_Native", Eagle::Script::Eagle_SceneComponent_GetWorldTransform);
		mono_add_internal_call("Eagle.SceneComponent::GetWorldLocation_Native", Eagle::Script::Eagle_SceneComponent_GetWorldLocation);
		mono_add_internal_call("Eagle.SceneComponent::GetWorldRotation_Native", Eagle::Script::Eagle_SceneComponent_GetWorldRotation);
		mono_add_internal_call("Eagle.SceneComponent::GetWorldScale_Native", Eagle::Script::Eagle_SceneComponent_GetWorldScale);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldTransform_Native", Eagle::Script::Eagle_SceneComponent_SetWorldTransform);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldLocation_Native", Eagle::Script::Eagle_SceneComponent_SetWorldLocation);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldRotation_Native", Eagle::Script::Eagle_SceneComponent_SetWorldRotation);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldScale_Native", Eagle::Script::Eagle_SceneComponent_SetWorldScale);

		mono_add_internal_call("Eagle.SceneComponent::GetRelativeTransform_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeTransform);
		mono_add_internal_call("Eagle.SceneComponent::GetRelativeLocation_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeLocation);
		mono_add_internal_call("Eagle.SceneComponent::GetRelativeRotation_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeRotation);
		mono_add_internal_call("Eagle.SceneComponent::GetRelativeScale_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeScale);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeTransform_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeTransform);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeLocation_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeLocation);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeRotation_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeRotation);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeScale_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeScale);

		mono_add_internal_call("Eagle.SceneComponent::GetForwardVector_Native", Eagle::Script::Eagle_SceneComponent_GetForwardVector);
		mono_add_internal_call("Eagle.SceneComponent::GetRightVector_Native", Eagle::Script::Eagle_SceneComponent_GetRightVector);
		mono_add_internal_call("Eagle.SceneComponent::GetUpVector_Native", Eagle::Script::Eagle_SceneComponent_GetUpVector);

		//Light Component
		mono_add_internal_call("Eagle.LightComponent::GetLightColor_Native", Eagle::Script::Eagle_LightComponent_GetLightColor);
		mono_add_internal_call("Eagle.LightComponent::GetIntensity_Native", Eagle::Script::Eagle_LightComponent_GetIntensity);
		mono_add_internal_call("Eagle.LightComponent::GetVolumetricFogIntensity_Native", Eagle::Script::Eagle_LightComponent_GetVolumetricFogIntensity);
		mono_add_internal_call("Eagle.LightComponent::GetAffectsWorld_Native", Eagle::Script::Eagle_LightComponent_GetAffectsWorld);
		mono_add_internal_call("Eagle.LightComponent::GetCastsShadows_Native", Eagle::Script::Eagle_LightComponent_GetCastsShadows);
		mono_add_internal_call("Eagle.LightComponent::SetLightColor_Native", Eagle::Script::Eagle_LightComponent_SetLightColor);
		mono_add_internal_call("Eagle.LightComponent::SetIntensity_Native", Eagle::Script::Eagle_LightComponent_SetIntensity);
		mono_add_internal_call("Eagle.LightComponent::SetVolumetricFogIntensity_Native", Eagle::Script::Eagle_LightComponent_SetVolumetricFogIntensity);
		mono_add_internal_call("Eagle.LightComponent::SetAffectsWorld_Native", Eagle::Script::Eagle_LightComponent_SetAffectsWorld);
		mono_add_internal_call("Eagle.LightComponent::SetCastsShadows_Native", Eagle::Script::Eagle_LightComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.LightComponent::GetIsVolumetricLight_Native", Eagle::Script::Eagle_LightComponent_GetIsVolumetricLight);
		mono_add_internal_call("Eagle.LightComponent::SetIsVolumetricLight_Native", Eagle::Script::Eagle_LightComponent_SetIsVolumetricLight);
		
		//PointLight Component
		mono_add_internal_call("Eagle.PointLightComponent::GetRadius_Native", Eagle::Script::Eagle_PointLightComponent_GetRadius);
		mono_add_internal_call("Eagle.PointLightComponent::SetRadius_Native", Eagle::Script::Eagle_PointLightComponent_SetRadius);

		//SpotLight Component
		mono_add_internal_call("Eagle.SpotLightComponent::GetInnerCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetInnerCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::GetOuterCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetOuterCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetInnerCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_SetInnerCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetOuterCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_SetOuterCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetDistance_Native", Eagle::Script::Eagle_SpotLightComponent_SetDistance);
		mono_add_internal_call("Eagle.SpotLightComponent::GetDistance_Native", Eagle::Script::Eagle_SpotLightComponent_GetDistance);
		
		// DirectionalLightComponent
		mono_add_internal_call("Eagle.DirectionalLightComponent::GetAmbient_Native", Eagle::Script::Eagle_DirectionalLightComponent_GetAmbient);
		mono_add_internal_call("Eagle.DirectionalLightComponent::SetAmbient_Native", Eagle::Script::Eagle_DirectionalLightComponent_SetAmbient);

		//StaticMeshComponent
		mono_add_internal_call("Eagle.StaticMeshComponent::SetMesh_Native", Eagle::Script::Eagle_StaticMeshComponent_SetMesh);
		mono_add_internal_call("Eagle.StaticMeshComponent::GetMesh_Native", Eagle::Script::Eagle_StaticMeshComponent_GetMesh);
		mono_add_internal_call("Eagle.StaticMeshComponent::GetMaterial_Native", Eagle::Script::Eagle_StaticMeshComponent_GetMaterial);
		mono_add_internal_call("Eagle.StaticMeshComponent::SetMaterial_Native", Eagle::Script::Eagle_StaticMeshComponent_SetMaterial);
		mono_add_internal_call("Eagle.StaticMeshComponent::GetMaterialsSlotsCount_Native", Eagle::Script::Eagle_StaticMeshComponent_GetMaterialsSlotsCount);
		mono_add_internal_call("Eagle.StaticMeshComponent::SetCastsShadows_Native", Eagle::Script::Eagle_StaticMeshComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.StaticMeshComponent::DoesCastShadows_Native", Eagle::Script::Eagle_StaticMeshComponent_DoesCastShadows);
		mono_add_internal_call("Eagle.StaticMeshComponent::SetReceivesDecals_Native", Eagle::Script::Eagle_StaticMeshComponent_SetReceivesDecals);
		mono_add_internal_call("Eagle.StaticMeshComponent::DoesReceiveDecals_Native", Eagle::Script::Eagle_StaticMeshComponent_DoesReceiveDecals);
		mono_add_internal_call("Eagle.StaticMeshComponent::SetVisible_Native", Eagle::Script::Eagle_StaticMeshComponent_SetVisible);
		mono_add_internal_call("Eagle.StaticMeshComponent::IsVisible_Native", Eagle::Script::Eagle_StaticMeshComponent_IsVisible);

		//SkeletalMeshComponent
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetMesh_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetMesh);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetMesh_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetMesh);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetMaterial_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetMaterial);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetMaterial_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetMaterial);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimation_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimation);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimation_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimation);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetMaterialsSlotsCount_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetMaterialsSlotsCount);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetCastsShadows_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::DoesCastShadows_Native", Eagle::Script::Eagle_SkeletalMeshComponent_DoesCastShadows);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimType_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimType);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimType_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimType);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetReceivesDecals_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetReceivesDecals);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::DoesReceiveDecals_Native", Eagle::Script::Eagle_SkeletalMeshComponent_DoesReceiveDecals);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetVisible_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetVisible);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::IsVisible_Native", Eagle::Script::Eagle_SkeletalMeshComponent_IsVisible);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimationGraph_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimationGraph);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimationGraph_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimationGraph);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::IsRootMotionLockFlagSet_Native", Eagle::Script::Eagle_SkeletalMeshComponent_IsRootMotionLockFlagSet);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRootMotionLockFlagBool_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRootMotionLockFlagBool);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRootMotionLockFlag_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRootMotionLockFlag);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetRootMotionLockFlags_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetRootMotionLockFlags);

		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetCurrentClipPlayTime_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetCurrentClipPlayTime);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetClipPlaybackSpeed_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetClipPlaybackSpeed);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetIsClipLooping_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetIsClipLooping);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetCurrentClipPlayTime_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetCurrentClipPlayTime);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetClipPlaybackSpeed_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetClipPlaybackSpeed);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::IsClipLooping_Native", Eagle::Script::Eagle_SkeletalMeshComponent_IsClipLooping);

		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimGraphVariableBool_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableBool);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimGraphVariableInt_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableInt);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimGraphVariableFloat_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableFloat);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimGraphVariableAnim_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableAnim);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimGraphVariableString_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableString);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetAnimGraphVariableVec4_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableVec4);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimGraphVariableBool_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableBool);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimGraphVariableInt_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableInt);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimGraphVariableFloat_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableFloat);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimGraphVariableAnim_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableAnim);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimGraphVariableString_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableString);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetAnimGraphVariableVec4_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableVec4);

		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRagdollEnabled_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRagdollEnabled);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::IsRagdollEnabled_Native", Eagle::Script::Eagle_SkeletalMeshComponent_IsRagdollEnabled);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetRagdollRootBoneWorldTransform_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetRagdollRootBoneWorldTransform);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetRagdollBoneWorldTransform_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetRagdollBoneWorldTransform);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetBoneWorldTransform_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetBoneWorldTransform);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetBoneWorldLocation_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetBoneWorldLocation);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetBoneWorldRotation_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetBoneWorldRotation);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetBoneWorldScale_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetBoneWorldScale);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRagdollCollisionVisible_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRagdollCollisionVisible);

		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetRagdollLinearVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetRagdollLinearVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetRagdollAngularVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetRagdollAngularVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRagdollLinearVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRagdollLinearVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRagdollAngularVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRagdollAngularVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::AddRagdollForce_Native", Eagle::Script::Eagle_SkeletalMeshComponent_AddRagdollForce);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::AddRagdollForceAtLocation_Native", Eagle::Script::Eagle_SkeletalMeshComponent_AddRagdollForceAtLocation);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::AddRagdollTorque_Native", Eagle::Script::Eagle_SkeletalMeshComponent_AddRagdollTorque);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRagdollBoneLinearVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRagdollBoneLinearVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::SetRagdollBoneAngularVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_SetRagdollBoneAngularVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetRagdollBoneLinearVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetRagdollBoneLinearVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::GetRagdollBoneAngularVelocity_Native", Eagle::Script::Eagle_SkeletalMeshComponent_GetRagdollBoneAngularVelocity);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::AddRagdollBoneForce_Native", Eagle::Script::Eagle_SkeletalMeshComponent_AddRagdollBoneForce);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::AddRagdollBoneForceAtLocation_Native", Eagle::Script::Eagle_SkeletalMeshComponent_AddRagdollBoneForceAtLocation);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::AddRagdollBoneTorque_Native", Eagle::Script::Eagle_SkeletalMeshComponent_AddRagdollBoneTorque);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::PutRagdollToSleep_Native", Eagle::Script::Eagle_SkeletalMeshComponent_PutRagdollToSleep);
		mono_add_internal_call("Eagle.SkeletalMeshComponent::WakeUpRagdoll_Native", Eagle::Script::Eagle_SkeletalMeshComponent_WakeUpRagdoll);

		//Sound
		mono_add_internal_call("Eagle.Sound::SetSettings_Native", Eagle::Script::Eagle_Sound_SetSettings);
		mono_add_internal_call("Eagle.Sound::GetSettings_Native", Eagle::Script::Eagle_Sound_GetSettings);
		mono_add_internal_call("Eagle.Sound::Play_Native", Eagle::Script::Eagle_Sound_Play);
		mono_add_internal_call("Eagle.Sound::Stop_Native", Eagle::Script::Eagle_Sound_Stop);
		mono_add_internal_call("Eagle.Sound::SetPaused_Native", Eagle::Script::Eagle_Sound_SetPaused);
		mono_add_internal_call("Eagle.Sound::IsPlaying_Native", Eagle::Script::Eagle_Sound_IsPlaying);
		mono_add_internal_call("Eagle.Sound::SetPosition_Native", Eagle::Script::Eagle_Sound_SetPosition);
		mono_add_internal_call("Eagle.Sound::GetPosition_Native", Eagle::Script::Eagle_Sound_GetPosition);
		mono_add_internal_call("Eagle.Sound::SetFFTEnabled_Native", Eagle::Script::Eagle_Sound_SetFFTEnabled);
		mono_add_internal_call("Eagle.Sound::IsFFTEnabled_Native", Eagle::Script::Eagle_Sound_IsFFTEnabled);
		mono_add_internal_call("Eagle.Sound::SetFFTSamples_Native", Eagle::Script::Eagle_Sound_SetFFTSamples);
		mono_add_internal_call("Eagle.Sound::GetFFTSamples_Native", Eagle::Script::Eagle_Sound_GetFFTSamples);
		mono_add_internal_call("Eagle.Sound::SetFFTType_Native", Eagle::Script::Eagle_Sound_SetFFTType);
		mono_add_internal_call("Eagle.Sound::GetFFTType_Native", Eagle::Script::Eagle_Sound_GetFFTType);
		mono_add_internal_call("Eagle.Sound::GetSpectrumData_Native", Eagle::Script::Eagle_Sound_GetSpectrumData);
		mono_add_internal_call("Eagle.Sound::GetSampleRate_Native", Eagle::Script::Eagle_Sound_GetSampleRate);
		mono_add_internal_call("Eagle.Sound::GetChannelsCount_Native", Eagle::Script::Eagle_Sound_GetChannelsCount);

		//Sound2D
		mono_add_internal_call("Eagle.Sound2D::Create_Native", Eagle::Script::Eagle_Sound2D_Create);

		//Sound3D
		mono_add_internal_call("Eagle.Sound3D::Create_Native", Eagle::Script::Eagle_Sound3D_Create);
		mono_add_internal_call("Eagle.Sound3D::SetMinDistance_Native", Eagle::Script::Eagle_Sound3D_SetMinDistance);
		mono_add_internal_call("Eagle.Sound3D::SetMaxDistance_Native", Eagle::Script::Eagle_Sound3D_SetMaxDistance);
		mono_add_internal_call("Eagle.Sound3D::SetMinMaxDistance_Native", Eagle::Script::Eagle_Sound3D_SetMinMaxDistance);
		mono_add_internal_call("Eagle.Sound3D::SetWorldPosition_Native", Eagle::Script::Eagle_Sound3D_SetWorldPosition);
		mono_add_internal_call("Eagle.Sound3D::SetVelocity_Native", Eagle::Script::Eagle_Sound3D_SetVelocity);
		mono_add_internal_call("Eagle.Sound3D::SetRollOffModel_Native", Eagle::Script::Eagle_Sound3D_SetRollOffModel);
		mono_add_internal_call("Eagle.Sound3D::GetMinDistance_Native", Eagle::Script::Eagle_Sound3D_GetMinDistance);
		mono_add_internal_call("Eagle.Sound3D::GetMaxDistance_Native", Eagle::Script::Eagle_Sound3D_GetMaxDistance);
		mono_add_internal_call("Eagle.Sound3D::GetWorldPosition_Native", Eagle::Script::Eagle_Sound3D_GetWorldPosition);
		mono_add_internal_call("Eagle.Sound3D::GetVelocity_Native", Eagle::Script::Eagle_Sound3D_GetVelocity);
		mono_add_internal_call("Eagle.Sound3D::GetRollOffModel_Native", Eagle::Script::Eagle_Sound3D_GetRollOffModel);

		//AudioComponent
		mono_add_internal_call("Eagle.AudioComponent::SetMinDistance_Native", Eagle::Script::Eagle_AudioComponent_SetMinDistance);
		mono_add_internal_call("Eagle.AudioComponent::SetMaxDistance_Native", Eagle::Script::Eagle_AudioComponent_SetMaxDistance);
		mono_add_internal_call("Eagle.AudioComponent::SetMinMaxDistance_Native", Eagle::Script::Eagle_AudioComponent_SetMinMaxDistance);
		mono_add_internal_call("Eagle.AudioComponent::SetRollOffModel_Native", Eagle::Script::Eagle_AudioComponent_SetRollOffModel);
		mono_add_internal_call("Eagle.AudioComponent::SetVolume_Native", Eagle::Script::Eagle_AudioComponent_SetVolume);
		mono_add_internal_call("Eagle.AudioComponent::SetPitch_Native", Eagle::Script::Eagle_AudioComponent_SetPitch);
		mono_add_internal_call("Eagle.AudioComponent::SetLoopCount_Native", Eagle::Script::Eagle_AudioComponent_SetLoopCount);
		mono_add_internal_call("Eagle.AudioComponent::SetLooping_Native", Eagle::Script::Eagle_AudioComponent_SetLooping);
		mono_add_internal_call("Eagle.AudioComponent::SetMuted_Native", Eagle::Script::Eagle_AudioComponent_SetMuted);
		mono_add_internal_call("Eagle.AudioComponent::SetAudioAsset_Native", Eagle::Script::Eagle_AudioComponent_SetAudioAsset);
		mono_add_internal_call("Eagle.AudioComponent::GetAudioAsset_Native", Eagle::Script::Eagle_AudioComponent_GetAudioAsset);
		mono_add_internal_call("Eagle.AudioComponent::SetStreaming_Native", Eagle::Script::Eagle_AudioComponent_SetStreaming);
		mono_add_internal_call("Eagle.AudioComponent::Play_Native", Eagle::Script::Eagle_AudioComponent_Play);
		mono_add_internal_call("Eagle.AudioComponent::Stop_Native", Eagle::Script::Eagle_AudioComponent_Stop);
		mono_add_internal_call("Eagle.AudioComponent::SetPaused_Native", Eagle::Script::Eagle_AudioComponent_SetPaused);
		mono_add_internal_call("Eagle.AudioComponent::SetDopplerEffectEnabled_Native", Eagle::Script::Eagle_AudioComponent_SetDopplerEffectEnabled);
		mono_add_internal_call("Eagle.AudioComponent::GetMinDistance_Native", Eagle::Script::Eagle_AudioComponent_GetMinDistance);
		mono_add_internal_call("Eagle.AudioComponent::GetMaxDistance_Native", Eagle::Script::Eagle_AudioComponent_GetMaxDistance);
		mono_add_internal_call("Eagle.AudioComponent::GetRollOffModel_Native", Eagle::Script::Eagle_AudioComponent_GetRollOffModel);
		mono_add_internal_call("Eagle.AudioComponent::GetVolume_Native", Eagle::Script::Eagle_AudioComponent_GetVolume);
		mono_add_internal_call("Eagle.AudioComponent::GetPitch_Native", Eagle::Script::Eagle_AudioComponent_GetPitch);
		mono_add_internal_call("Eagle.AudioComponent::GetLoopCount_Native", Eagle::Script::Eagle_AudioComponent_GetLoopCount);
		mono_add_internal_call("Eagle.AudioComponent::IsLooping_Native", Eagle::Script::Eagle_AudioComponent_IsLooping);
		mono_add_internal_call("Eagle.AudioComponent::IsMuted_Native", Eagle::Script::Eagle_AudioComponent_IsMuted);
		mono_add_internal_call("Eagle.AudioComponent::IsStreaming_Native", Eagle::Script::Eagle_AudioComponent_IsStreaming);
		mono_add_internal_call("Eagle.AudioComponent::IsPlaying_Native", Eagle::Script::Eagle_AudioComponent_IsPlaying);
		mono_add_internal_call("Eagle.AudioComponent::IsDopplerEffectEnabled_Native", Eagle::Script::Eagle_AudioComponent_IsDopplerEffectEnabled);
		mono_add_internal_call("Eagle.AudioComponent::SetFFTEnabled_Native", Eagle::Script::Eagle_AudioComponent_SetFFTEnabled);
		mono_add_internal_call("Eagle.AudioComponent::IsFFTEnabled_Native", Eagle::Script::Eagle_AudioComponent_IsFFTEnabled);
		mono_add_internal_call("Eagle.AudioComponent::SetFFTSamples_Native", Eagle::Script::Eagle_AudioComponent_SetFFTSamples);
		mono_add_internal_call("Eagle.AudioComponent::GetFFTSamples_Native", Eagle::Script::Eagle_AudioComponent_GetFFTSamples);
		mono_add_internal_call("Eagle.AudioComponent::SetFFTType_Native", Eagle::Script::Eagle_AudioComponent_SetFFTType);
		mono_add_internal_call("Eagle.AudioComponent::GetFFTType_Native", Eagle::Script::Eagle_AudioComponent_GetFFTType);
		mono_add_internal_call("Eagle.AudioComponent::GetSpectrumData_Native", Eagle::Script::Eagle_AudioComponent_GetSpectrumData);
		mono_add_internal_call("Eagle.AudioComponent::GetSampleRate_Native", Eagle::Script::Eagle_AudioComponent_GetSampleRate);
		mono_add_internal_call("Eagle.AudioComponent::SetPosition_Native", Eagle::Script::Eagle_AudioComponent_SetPosition);
		mono_add_internal_call("Eagle.AudioComponent::GetPosition_Native", Eagle::Script::Eagle_AudioComponent_GetPosition);
		mono_add_internal_call("Eagle.AudioComponent::SetIs3D_Native", Eagle::Script::Eagle_AudioComponent_SetIs3D);
		mono_add_internal_call("Eagle.AudioComponent::Is3D_Native", Eagle::Script::Eagle_AudioComponent_Is3D);
		mono_add_internal_call("Eagle.AudioComponent::SetPan_Native", Eagle::Script::Eagle_AudioComponent_SetPan);
		mono_add_internal_call("Eagle.AudioComponent::GetPan_Native", Eagle::Script::Eagle_AudioComponent_GetPan);
		mono_add_internal_call("Eagle.AudioComponent::GetChannelsCount_Native", Eagle::Script::Eagle_AudioComponent_GetChannelsCount);

		//RigidBodyComponent
		mono_add_internal_call("Eagle.RigidBodyComponent::SetBodyType_Native", Eagle::Script::Eagle_RigidBodyComponent_SetBodyType);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetBodyType_Native", Eagle::Script::Eagle_RigidBodyComponent_GetBodyType);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetCollisionDetectionType_Native", Eagle::Script::Eagle_RigidBodyComponent_SetCollisionDetectionType);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetCollisionDetectionType_Native", Eagle::Script::Eagle_RigidBodyComponent_GetCollisionDetectionType);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetPositionSolverIterations_Native", Eagle::Script::Eagle_RigidBodyComponent_SetPositionSolverIterations);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetVelocitySolverIterations_Native", Eagle::Script::Eagle_RigidBodyComponent_SetVelocitySolverIterations);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetPositionSolverIterations_Native", Eagle::Script::Eagle_RigidBodyComponent_GetPositionSolverIterations);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetVelocitySolverIterations_Native", Eagle::Script::Eagle_RigidBodyComponent_GetVelocitySolverIterations);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetMass_Native", Eagle::Script::Eagle_RigidBodyComponent_SetMass);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetMass_Native", Eagle::Script::Eagle_RigidBodyComponent_GetMass);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLinearDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLinearDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetLinearDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_GetLinearDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetAngularDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_SetAngularDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetAngularDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_GetAngularDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetEnableGravity_Native", Eagle::Script::Eagle_RigidBodyComponent_SetEnableGravity);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsGravityEnabled_Native", Eagle::Script::Eagle_RigidBodyComponent_IsGravityEnabled);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetIsKinematic_Native", Eagle::Script::Eagle_RigidBodyComponent_SetIsKinematic);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsKinematic_Native", Eagle::Script::Eagle_RigidBodyComponent_IsKinematic);
		mono_add_internal_call("Eagle.RigidBodyComponent::WakeUp_Native", Eagle::Script::Eagle_RigidBodyComponent_WakeUp);
		mono_add_internal_call("Eagle.RigidBodyComponent::PutToSleep_Native", Eagle::Script::Eagle_RigidBodyComponent_PutToSleep);
		mono_add_internal_call("Eagle.RigidBodyComponent::AddForce_Native", Eagle::Script::Eagle_RigidBodyComponent_AddForce);
		mono_add_internal_call("Eagle.RigidBodyComponent::AddForceAtLocation_Native", Eagle::Script::Eagle_RigidBodyComponent_AddForceAtLocation);
		mono_add_internal_call("Eagle.RigidBodyComponent::AddTorque_Native", Eagle::Script::Eagle_RigidBodyComponent_AddTorque);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetLinearVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_GetLinearVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLinearVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLinearVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetAngularVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_GetAngularVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetAngularVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_SetAngularVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetMaxLinearVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_GetMaxLinearVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetMaxLinearVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_SetMaxLinearVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetMaxAngularVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_GetMaxAngularVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetMaxAngularVelocity_Native", Eagle::Script::Eagle_RigidBodyComponent_SetMaxAngularVelocity);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsDynamic_Native", Eagle::Script::Eagle_RigidBodyComponent_IsDynamic);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetKinematicTarget_Native", Eagle::Script::Eagle_RigidBodyComponent_GetKinematicTarget);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetKinematicTargetLocation_Native", Eagle::Script::Eagle_RigidBodyComponent_GetKinematicTargetLocation);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetKinematicTargetRotation_Native", Eagle::Script::Eagle_RigidBodyComponent_GetKinematicTargetRotation);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetKinematicTarget_Native", Eagle::Script::Eagle_RigidBodyComponent_SetKinematicTarget);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetKinematicTargetLocation_Native", Eagle::Script::Eagle_RigidBodyComponent_SetKinematicTargetLocation);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetKinematicTargetRotation_Native", Eagle::Script::Eagle_RigidBodyComponent_SetKinematicTargetRotation);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsLockFlagSet_Native", Eagle::Script::Eagle_RigidBodyComponent_IsLockFlagSet);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetLockFlags_Native", Eagle::Script::Eagle_RigidBodyComponent_GetLockFlags);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockFlag_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockFlag);

		//BaseColliderComponent
		mono_add_internal_call("Eagle.BaseColliderComponent::SetCollisionGroup_Native", Eagle::Script::Eagle_BaseColliderComponent_SetCollisionGroup);
		mono_add_internal_call("Eagle.BaseColliderComponent::GetCollisionGroup_Native", Eagle::Script::Eagle_BaseColliderComponent_GetCollisionGroup);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetInteractingCollisionGroup_Native", Eagle::Script::Eagle_BaseColliderComponent_SetInteractingCollisionGroup);
		mono_add_internal_call("Eagle.BaseColliderComponent::GetInteractingCollisionGroup_Native", Eagle::Script::Eagle_BaseColliderComponent_GetInteractingCollisionGroup);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetIsTrigger_Native", Eagle::Script::Eagle_BaseColliderComponent_SetIsTrigger);
		mono_add_internal_call("Eagle.BaseColliderComponent::IsTrigger_Native", Eagle::Script::Eagle_BaseColliderComponent_IsTrigger);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetCollisionEnabled_Native", Eagle::Script::Eagle_BaseColliderComponent_SetCollisionEnabled);
		mono_add_internal_call("Eagle.BaseColliderComponent::IsCollisionEnabled_Native", Eagle::Script::Eagle_BaseColliderComponent_IsCollisionEnabled);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetCollisionVisible_Native", Eagle::Script::Eagle_BaseColliderComponent_SetCollisionVisible);
		mono_add_internal_call("Eagle.BaseColliderComponent::IsCollisionVisible_Native", Eagle::Script::Eagle_BaseColliderComponent_IsCollisionVisible);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetPhysicsMaterial_Native", Eagle::Script::Eagle_BaseColliderComponent_SetPhysicsMaterial);
		mono_add_internal_call("Eagle.BaseColliderComponent::GetPhysicsMaterial_Native", Eagle::Script::Eagle_BaseColliderComponent_GetPhysicsMaterial);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetAffectsNavMeshBuild_Native", Eagle::Script::Eagle_BaseColliderComponent_SetAffectsNavMeshBuild);
		mono_add_internal_call("Eagle.BaseColliderComponent::DoesAffectNavMeshBuild_Native", Eagle::Script::Eagle_BaseColliderComponent_DoesAffectNavMeshBuild);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetIsObstacle_Native", Eagle::Script::Eagle_BaseColliderComponent_SetIsObstacle);
		mono_add_internal_call("Eagle.BaseColliderComponent::IsObstacle_Native", Eagle::Script::Eagle_BaseColliderComponent_IsObstacle);

		//BoxColliderComponent
		mono_add_internal_call("Eagle.BoxColliderComponent::SetSize_Native", Eagle::Script::Eagle_BoxColliderComponent_SetSize);
		mono_add_internal_call("Eagle.BoxColliderComponent::GetSize_Native", Eagle::Script::Eagle_BoxColliderComponent_GetSize);

		//SphereColliderComponent
		mono_add_internal_call("Eagle.SphereColliderComponent::SetRadius_Native", Eagle::Script::Eagle_SphereColliderComponent_SetRadius);
		mono_add_internal_call("Eagle.SphereColliderComponent::GetRadius_Native", Eagle::Script::Eagle_SphereColliderComponent_GetRadius);

		//CapsuleColliderComponent
		mono_add_internal_call("Eagle.CapsuleColliderComponent::SetRadius_Native", Eagle::Script::Eagle_CapsuleColliderComponent_SetRadius);
		mono_add_internal_call("Eagle.CapsuleColliderComponent::GetRadius_Native", Eagle::Script::Eagle_CapsuleColliderComponent_GetRadius);
		mono_add_internal_call("Eagle.CapsuleColliderComponent::SetHeight_Native", Eagle::Script::Eagle_CapsuleColliderComponent_SetHeight);
		mono_add_internal_call("Eagle.CapsuleColliderComponent::GetHeight_Native", Eagle::Script::Eagle_CapsuleColliderComponent_GetHeight);

		//MeshColliderComponent
		mono_add_internal_call("Eagle.MeshColliderComponent::SetIsConvex_Native", Eagle::Script::Eagle_MeshColliderComponent_SetIsConvex);
		mono_add_internal_call("Eagle.MeshColliderComponent::IsConvex_Native", Eagle::Script::Eagle_MeshColliderComponent_IsConvex);
		mono_add_internal_call("Eagle.MeshColliderComponent::SetIsTwoSided_Native", Eagle::Script::Eagle_MeshColliderComponent_SetIsTwoSided);
		mono_add_internal_call("Eagle.MeshColliderComponent::IsTwoSided_Native", Eagle::Script::Eagle_MeshColliderComponent_IsTwoSided);
		mono_add_internal_call("Eagle.MeshColliderComponent::SetCollisionMesh_Native", Eagle::Script::Eagle_MeshColliderComponent_SetCollisionMesh);
		mono_add_internal_call("Eagle.MeshColliderComponent::GetCollisionMesh_Native", Eagle::Script::Eagle_MeshColliderComponent_GetCollisionMesh);

		// Camera Component
		mono_add_internal_call("Eagle.CameraComponent::SetIsPrimary_Native", Eagle::Script::Eagle_CameraComponent_SetIsPrimary);
		mono_add_internal_call("Eagle.CameraComponent::GetIsPrimary_Native", Eagle::Script::Eagle_CameraComponent_GetIsPrimary);
		mono_add_internal_call("Eagle.CameraComponent::SetPerspectiveVerticalFOV_Native", Eagle::Script::Eagle_CameraComponent_SetPerspectiveVerticalFOV);
		mono_add_internal_call("Eagle.CameraComponent::GetPerspectiveVerticalFOV_Native", Eagle::Script::Eagle_CameraComponent_GetPerspectiveVerticalFOV);
		mono_add_internal_call("Eagle.CameraComponent::SetPerspectiveNearClip_Native", Eagle::Script::Eagle_CameraComponent_SetPerspectiveNearClip);
		mono_add_internal_call("Eagle.CameraComponent::GetPerspectiveNearClip_Native", Eagle::Script::Eagle_CameraComponent_GetPerspectiveNearClip);
		mono_add_internal_call("Eagle.CameraComponent::SetPerspectiveFarClip_Native", Eagle::Script::Eagle_CameraComponent_SetPerspectiveFarClip);
		mono_add_internal_call("Eagle.CameraComponent::GetPerspectiveFarClip_Native", Eagle::Script::Eagle_CameraComponent_GetPerspectiveFarClip);
		mono_add_internal_call("Eagle.CameraComponent::SetShadowFarClip_Native", Eagle::Script::Eagle_CameraComponent_SetShadowFarClip);
		mono_add_internal_call("Eagle.CameraComponent::GetShadowFarClip_Native", Eagle::Script::Eagle_CameraComponent_GetShadowFarClip);
		mono_add_internal_call("Eagle.CameraComponent::SetCascadesSplitAlpha_Native", Eagle::Script::Eagle_CameraComponent_SetCascadesSplitAlpha);
		mono_add_internal_call("Eagle.CameraComponent::GetCascadesSplitAlpha_Native", Eagle::Script::Eagle_CameraComponent_GetCascadesSplitAlpha);
		mono_add_internal_call("Eagle.CameraComponent::GetCascadesSmoothTransitionAlpha_Native", Eagle::Script::Eagle_CameraComponent_GetCascadesSmoothTransitionAlpha);
		mono_add_internal_call("Eagle.CameraComponent::SetCascadesSmoothTransitionAlpha_Native", Eagle::Script::Eagle_CameraComponent_SetCascadesSmoothTransitionAlpha);
		mono_add_internal_call("Eagle.CameraComponent::GetCameraProjectionMode_Native", Eagle::Script::Eagle_CameraComponent_GetCameraProjectionMode);
		mono_add_internal_call("Eagle.CameraComponent::SetCameraProjectionMode_Native", Eagle::Script::Eagle_CameraComponent_SetCameraProjectionMode);
		mono_add_internal_call("Eagle.CameraComponent::GetAspectRatio_Native", Eagle::Script::Eagle_CameraComponent_GetAspectRatio);

		// Reverb component
		mono_add_internal_call("Eagle.ReverbComponent::IsActive_Native", Eagle::Script::Eagle_ReverbComponent_IsActive);
		mono_add_internal_call("Eagle.ReverbComponent::SetIsActive_Native", Eagle::Script::Eagle_ReverbComponent_SetIsActive);
		mono_add_internal_call("Eagle.ReverbComponent::GetReverbPreset_Native", Eagle::Script::Eagle_ReverbComponent_GetReverbPreset);
		mono_add_internal_call("Eagle.ReverbComponent::SetReverbPreset_Native", Eagle::Script::Eagle_ReverbComponent_SetReverbPreset);
		mono_add_internal_call("Eagle.ReverbComponent::GetMinDistance_Native", Eagle::Script::Eagle_ReverbComponent_GetMinDistance);
		mono_add_internal_call("Eagle.ReverbComponent::SetMinDistance_Native", Eagle::Script::Eagle_ReverbComponent_SetMinDistance);
		mono_add_internal_call("Eagle.ReverbComponent::GetMaxDistance_Native", Eagle::Script::Eagle_ReverbComponent_GetMaxDistance);
		mono_add_internal_call("Eagle.ReverbComponent::SetMaxDistance_Native", Eagle::Script::Eagle_ReverbComponent_SetMaxDistance);

		// Text Component
		mono_add_internal_call("Eagle.TextComponent::GetText_Native", Eagle::Script::Eagle_TextComponent_GetText);
		mono_add_internal_call("Eagle.TextComponent::SetText_Native", Eagle::Script::Eagle_TextComponent_SetText);
		mono_add_internal_call("Eagle.TextComponent::GetColor_Native", Eagle::Script::Eagle_TextComponent_GetColor);
		mono_add_internal_call("Eagle.TextComponent::SetColor_Native", Eagle::Script::Eagle_TextComponent_SetColor);
		mono_add_internal_call("Eagle.TextComponent::GetLineSpacing_Native", Eagle::Script::Eagle_TextComponent_GetLineSpacing);
		mono_add_internal_call("Eagle.TextComponent::SetLineSpacing_Native", Eagle::Script::Eagle_TextComponent_SetLineSpacing);
		mono_add_internal_call("Eagle.TextComponent::GetKerning_Native", Eagle::Script::Eagle_TextComponent_GetKerning);
		mono_add_internal_call("Eagle.TextComponent::SetKerning_Native", Eagle::Script::Eagle_TextComponent_SetKerning);
		mono_add_internal_call("Eagle.TextComponent::GetMaxWidth_Native", Eagle::Script::Eagle_TextComponent_GetMaxWidth);
		mono_add_internal_call("Eagle.TextComponent::SetMaxWidth_Native", Eagle::Script::Eagle_TextComponent_SetMaxWidth);
		mono_add_internal_call("Eagle.TextComponent::GetIsLit_Native", Eagle::Script::Eagle_TextComponent_GetIsLit);
		mono_add_internal_call("Eagle.TextComponent::SetIsLit_Native", Eagle::Script::Eagle_TextComponent_SetIsLit);
		mono_add_internal_call("Eagle.TextComponent::SetCastsShadows_Native", Eagle::Script::Eagle_TextComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.TextComponent::DoesCastShadows_Native", Eagle::Script::Eagle_TextComponent_DoesCastShadows);
		mono_add_internal_call("Eagle.TextComponent::SetFont_Native", Eagle::Script::Eagle_TextComponent_SetFont);
		mono_add_internal_call("Eagle.TextComponent::GetFont_Native", Eagle::Script::Eagle_TextComponent_GetFont);
		mono_add_internal_call("Eagle.TextComponent::SetMaterial_Native", Eagle::Script::Eagle_TextComponent_SetMaterial);
		mono_add_internal_call("Eagle.TextComponent::GetMaterial_Native", Eagle::Script::Eagle_TextComponent_GetMaterial);
		mono_add_internal_call("Eagle.TextComponent::SetReceivesDecals_Native", Eagle::Script::Eagle_TextComponent_SetReceivesDecals);
		mono_add_internal_call("Eagle.TextComponent::DoesReceiveDecals_Native", Eagle::Script::Eagle_TextComponent_DoesReceiveDecals);
		mono_add_internal_call("Eagle.TextComponent::SetVisible_Native", Eagle::Script::Eagle_TextComponent_SetVisible);
		mono_add_internal_call("Eagle.TextComponent::IsVisible_Native", Eagle::Script::Eagle_TextComponent_IsVisible);
		mono_add_internal_call("Eagle.TextComponent::SetDoubleSided_Native", Eagle::Script::Eagle_TextComponent_SetDoubleSided);
		mono_add_internal_call("Eagle.TextComponent::IsDoubleSided_Native", Eagle::Script::Eagle_TextComponent_IsDoubleSided);

		// Text2D Component
		mono_add_internal_call("Eagle.Text2DComponent::GetText_Native", Eagle::Script::Eagle_Text2DComponent_GetText);
		mono_add_internal_call("Eagle.Text2DComponent::SetText_Native", Eagle::Script::Eagle_Text2DComponent_SetText);
		mono_add_internal_call("Eagle.Text2DComponent::GetColor_Native", Eagle::Script::Eagle_Text2DComponent_GetColor);
		mono_add_internal_call("Eagle.Text2DComponent::SetColor_Native", Eagle::Script::Eagle_Text2DComponent_SetColor);
		mono_add_internal_call("Eagle.Text2DComponent::GetLineSpacing_Native", Eagle::Script::Eagle_Text2DComponent_GetLineSpacing);
		mono_add_internal_call("Eagle.Text2DComponent::SetLineSpacing_Native", Eagle::Script::Eagle_Text2DComponent_SetLineSpacing);
		mono_add_internal_call("Eagle.Text2DComponent::GetKerning_Native", Eagle::Script::Eagle_Text2DComponent_GetKerning);
		mono_add_internal_call("Eagle.Text2DComponent::SetKerning_Native", Eagle::Script::Eagle_Text2DComponent_SetKerning);
		mono_add_internal_call("Eagle.Text2DComponent::GetMaxWidth_Native", Eagle::Script::Eagle_Text2DComponent_GetMaxWidth);
		mono_add_internal_call("Eagle.Text2DComponent::SetMaxWidth_Native", Eagle::Script::Eagle_Text2DComponent_SetMaxWidth);
		mono_add_internal_call("Eagle.Text2DComponent::SetOpacity_Native", Eagle::Script::Eagle_Text2DComponent_SetOpacity);
		mono_add_internal_call("Eagle.Text2DComponent::GetOpacity_Native", Eagle::Script::Eagle_Text2DComponent_GetOpacity);
		mono_add_internal_call("Eagle.Text2DComponent::SetRotation_Native", Eagle::Script::Eagle_Text2DComponent_SetRotation);
		mono_add_internal_call("Eagle.Text2DComponent::GetRotation_Native", Eagle::Script::Eagle_Text2DComponent_GetRotation);
		mono_add_internal_call("Eagle.Text2DComponent::SetPosition_Native", Eagle::Script::Eagle_Text2DComponent_SetPosition);
		mono_add_internal_call("Eagle.Text2DComponent::GetPosition_Native", Eagle::Script::Eagle_Text2DComponent_GetPosition);
		mono_add_internal_call("Eagle.Text2DComponent::SetScale_Native", Eagle::Script::Eagle_Text2DComponent_SetScale);
		mono_add_internal_call("Eagle.Text2DComponent::GetScale_Native", Eagle::Script::Eagle_Text2DComponent_GetScale);
		mono_add_internal_call("Eagle.Text2DComponent::SetIsVisible_Native", Eagle::Script::Eagle_Text2DComponent_SetIsVisible);
		mono_add_internal_call("Eagle.Text2DComponent::IsVisible_Native", Eagle::Script::Eagle_Text2DComponent_IsVisible);
		mono_add_internal_call("Eagle.Text2DComponent::SetFont_Native", Eagle::Script::Eagle_Text2DComponent_SetFont);
		mono_add_internal_call("Eagle.Text2DComponent::GetFont_Native", Eagle::Script::Eagle_Text2DComponent_GetFont);

		// Image2D Component
		mono_add_internal_call("Eagle.Image2DComponent::GetTexture_Native", Eagle::Script::Eagle_Image2DComponent_GetTexture);
		mono_add_internal_call("Eagle.Image2DComponent::SetTexture_Native", Eagle::Script::Eagle_Image2DComponent_SetTexture);
		mono_add_internal_call("Eagle.Image2DComponent::GetTint_Native", Eagle::Script::Eagle_Image2DComponent_GetTint);
		mono_add_internal_call("Eagle.Image2DComponent::SetTint_Native", Eagle::Script::Eagle_Image2DComponent_SetTint);
		mono_add_internal_call("Eagle.Image2DComponent::SetOpacity_Native", Eagle::Script::Eagle_Image2DComponent_SetOpacity);
		mono_add_internal_call("Eagle.Image2DComponent::GetOpacity_Native", Eagle::Script::Eagle_Image2DComponent_GetOpacity);
		mono_add_internal_call("Eagle.Image2DComponent::SetRotation_Native", Eagle::Script::Eagle_Image2DComponent_SetRotation);
		mono_add_internal_call("Eagle.Image2DComponent::GetRotation_Native", Eagle::Script::Eagle_Image2DComponent_GetRotation);
		mono_add_internal_call("Eagle.Image2DComponent::SetPosition_Native", Eagle::Script::Eagle_Image2DComponent_SetPosition);
		mono_add_internal_call("Eagle.Image2DComponent::GetPosition_Native", Eagle::Script::Eagle_Image2DComponent_GetPosition);
		mono_add_internal_call("Eagle.Image2DComponent::SetScale_Native", Eagle::Script::Eagle_Image2DComponent_SetScale);
		mono_add_internal_call("Eagle.Image2DComponent::GetScale_Native", Eagle::Script::Eagle_Image2DComponent_GetScale);
		mono_add_internal_call("Eagle.Image2DComponent::SetIsVisible_Native", Eagle::Script::Eagle_Image2DComponent_SetIsVisible);
		mono_add_internal_call("Eagle.Image2DComponent::IsVisible_Native", Eagle::Script::Eagle_Image2DComponent_IsVisible);

		// Billboard Component
		mono_add_internal_call("Eagle.BillboardComponent::SetTexture_Native", Eagle::Script::Eagle_BillboardComponent_SetTexture);
		mono_add_internal_call("Eagle.BillboardComponent::GetTexture_Native", Eagle::Script::Eagle_BillboardComponent_GetTexture);
		mono_add_internal_call("Eagle.BillboardComponent::SetVisible_Native", Eagle::Script::Eagle_BillboardComponent_SetVisible);
		mono_add_internal_call("Eagle.BillboardComponent::IsVisible_Native", Eagle::Script::Eagle_BillboardComponent_IsVisible);

		// Particle System Component
		mono_add_internal_call("Eagle.ParticleSystemComponent::Spawn_Native", Eagle::Script::Eagle_ParticleSystemComponent_Spawn);
		mono_add_internal_call("Eagle.ParticleSystemComponent::Destroy_Native", Eagle::Script::Eagle_ParticleSystemComponent_Destroy);
		mono_add_internal_call("Eagle.ParticleSystemComponent::SetAsset_Native", Eagle::Script::Eagle_ParticleSystemComponent_SetAsset);
		mono_add_internal_call("Eagle.ParticleSystemComponent::GetAsset_Native", Eagle::Script::Eagle_ParticleSystemComponent_GetAsset);
		mono_add_internal_call("Eagle.ParticleSystemComponent::DuplicatePose_Native", Eagle::Script::Eagle_ParticleSystemComponent_DuplicatePose);

		// Decal Component
		mono_add_internal_call("Eagle.DecalComponent::SetMaterial_Native", Eagle::Script::Eagle_DecalComponent_SetMaterial);
		mono_add_internal_call("Eagle.DecalComponent::GetMaterial_Native", Eagle::Script::Eagle_DecalComponent_GetMaterial);
		mono_add_internal_call("Eagle.DecalComponent::SetAdjustAspectRatioEnabled_Native", Eagle::Script::Eagle_DecalComponent_SetAdjustAspectRatioEnabled);
		mono_add_internal_call("Eagle.DecalComponent::IsAdjustAspectRatioEnabled_Native", Eagle::Script::Eagle_DecalComponent_IsAdjustAspectRatioEnabled);
		mono_add_internal_call("Eagle.DecalComponent::SetSortPriority_Native", Eagle::Script::Eagle_DecalComponent_SetSortPriority);
		mono_add_internal_call("Eagle.DecalComponent::GetSortPriority_Native", Eagle::Script::Eagle_DecalComponent_GetSortPriority);
		mono_add_internal_call("Eagle.DecalComponent::SetVisible_Native", Eagle::Script::Eagle_DecalComponent_SetVisible);
		mono_add_internal_call("Eagle.DecalComponent::IsVisible_Native", Eagle::Script::Eagle_DecalComponent_IsVisible);

		// NavigationMesh Component
		mono_add_internal_call("Eagle.NavigationMeshComponent::Build_Native", Eagle::Script::Eagle_NavigationMeshComponent_Build);
		mono_add_internal_call("Eagle.NavigationMeshComponent::SetCrowdSettings_Native", Eagle::Script::Eagle_NavigationMeshComponent_SetCrowdSettings);
		mono_add_internal_call("Eagle.NavigationMeshComponent::GetCrowdSettings_Native", Eagle::Script::Eagle_NavigationMeshComponent_GetCrowdSettings);
		mono_add_internal_call("Eagle.NavigationMeshComponent::GetSettings_Native", Eagle::Script::Eagle_NavigationMeshComponent_GetSettings);
		mono_add_internal_call("Eagle.NavigationMeshComponent::SetSettings_Native", Eagle::Script::Eagle_NavigationMeshComponent_SetSettings);
		mono_add_internal_call("Eagle.NavigationMeshComponent::SetAutoRebuild_Native", Eagle::Script::Eagle_NavigationMeshComponent_SetAutoRebuild);
		mono_add_internal_call("Eagle.NavigationMeshComponent::GetAutoRebuild_Native", Eagle::Script::Eagle_NavigationMeshComponent_GetAutoRebuild);

		// NavigationCrowdAgent Component
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::TeleportAgent_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_TeleportAgent);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::SetMoveTarget_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_SetMoveTarget);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::ResetMoveTarget_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_ResetMoveTarget);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::IsValid_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_IsValid);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::SetSettings_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_SetSettings);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::GetSettings_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_GetSettings);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::GetLocation_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_GetLocation);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::GetVelocity_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_GetVelocity);
		mono_add_internal_call("Eagle.NavigationCrowdAgentComponent::GetTargetState_Native", Eagle::Script::Eagle_NavigationCrowdAgentComponent_GetTargetState);

		// Sprite Component
		mono_add_internal_call("Eagle.SpriteComponent::GetMaterial_Native", Eagle::Script::Eagle_SpriteComponent_GetMaterial);
		mono_add_internal_call("Eagle.SpriteComponent::SetMaterial_Native", Eagle::Script::Eagle_SpriteComponent_SetMaterial);
		mono_add_internal_call("Eagle.SpriteComponent::GetAtlasSpriteCoords_Native", Eagle::Script::Eagle_SpriteComponent_GetAtlasSpriteCoords);
		mono_add_internal_call("Eagle.SpriteComponent::SetAtlasSpriteCoords_Native", Eagle::Script::Eagle_SpriteComponent_SetAtlasSpriteCoords);
		mono_add_internal_call("Eagle.SpriteComponent::GetAtlasSpriteSize_Native", Eagle::Script::Eagle_SpriteComponent_GetAtlasSpriteSize);
		mono_add_internal_call("Eagle.SpriteComponent::SetAtlasSpriteSize_Native", Eagle::Script::Eagle_SpriteComponent_SetAtlasSpriteSize);
		mono_add_internal_call("Eagle.SpriteComponent::GetAtlasSpriteSizeCoef_Native", Eagle::Script::Eagle_SpriteComponent_GetAtlasSpriteSizeCoef);
		mono_add_internal_call("Eagle.SpriteComponent::SetAtlasSpriteSizeCoef_Native", Eagle::Script::Eagle_SpriteComponent_SetAtlasSpriteSizeCoef);
		mono_add_internal_call("Eagle.SpriteComponent::GetIsAtlas_Native", Eagle::Script::Eagle_SpriteComponent_GetIsAtlas);
		mono_add_internal_call("Eagle.SpriteComponent::SetIsAtlas_Native", Eagle::Script::Eagle_SpriteComponent_SetIsAtlas);
		mono_add_internal_call("Eagle.SpriteComponent::SetCastsShadows_Native", Eagle::Script::Eagle_SpriteComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.SpriteComponent::DoesCastShadows_Native", Eagle::Script::Eagle_SpriteComponent_DoesCastShadows);
		mono_add_internal_call("Eagle.SpriteComponent::SetReceivesDecals_Native", Eagle::Script::Eagle_SpriteComponent_SetReceivesDecals);
		mono_add_internal_call("Eagle.SpriteComponent::DoesReceiveDecals_Native", Eagle::Script::Eagle_SpriteComponent_DoesReceiveDecals);
		mono_add_internal_call("Eagle.SpriteComponent::SetVisible_Native", Eagle::Script::Eagle_SpriteComponent_SetVisible);
		mono_add_internal_call("Eagle.SpriteComponent::IsVisible_Native", Eagle::Script::Eagle_SpriteComponent_IsVisible);

		// Project
		mono_add_internal_call("Eagle.Project::GetProjectPath_Native", Eagle::Script::Eagle_Project_GetProjectPath);
		mono_add_internal_call("Eagle.Project::GetBinariesPath_Native", Eagle::Script::Eagle_Project_GetBinariesPath);
		mono_add_internal_call("Eagle.Project::GetConfigPath_Native", Eagle::Script::Eagle_Project_GetConfigPath);
		mono_add_internal_call("Eagle.Project::GetCachePath_Native", Eagle::Script::Eagle_Project_GetCachePath);
		mono_add_internal_call("Eagle.Project::GetRendererCachePath_Native", Eagle::Script::Eagle_Project_GetRendererCachePath);
		mono_add_internal_call("Eagle.Project::GetSavedPath_Native", Eagle::Script::Eagle_Project_GetSavedPath);

		// Scene
		mono_add_internal_call("Eagle.Scene::OpenScene_Native", Eagle::Script::Eagle_Scene_OpenScene);
		mono_add_internal_call("Eagle.Scene::QuitGame_Native", Eagle::Script::Eagle_Scene_QuitGame);
		mono_add_internal_call("Eagle.Scene::Raycast_Native", Eagle::Script::Eagle_Scene_Raycast);
		mono_add_internal_call("Eagle.Scene::OverlapBox_Native", Eagle::Script::Eagle_Scene_OverlapBox);
		mono_add_internal_call("Eagle.Scene::OverlapCapsule_Native", Eagle::Script::Eagle_Scene_OverlapCapsule);
		mono_add_internal_call("Eagle.Scene::OverlapSphere_Native", Eagle::Script::Eagle_Scene_OverlapSphere);
		mono_add_internal_call("Eagle.Scene::SetGravity_Native", Eagle::Script::Eagle_Scene_SetGravity);
		mono_add_internal_call("Eagle.Scene::GetGravity_Native", Eagle::Script::Eagle_Scene_GetGravity);
		mono_add_internal_call("Eagle.Scene::GetAllEntitiesWithComponent_Native", Eagle::Script::Eagle_Scene_GetAllEntitiesWithComponent);
		mono_add_internal_call("Eagle.Scene::SpawnEntity_Native", Eagle::Script::Eagle_Scene_SpawnEntity);
		mono_add_internal_call("Eagle.Scene::SpawnEntityFromAsset_Native", Eagle::Script::Eagle_Scene_SpawnEntityFromAsset);
		mono_add_internal_call("Eagle.Scene::SpawnParticleSystem_Native", Eagle::Script::Eagle_Scene_SpawnParticleSystem);

		// Navigation
		mono_add_internal_call("Eagle.Navigation::FindStraightPath_Native", Eagle::Script::Eagle_Navigation_FindStraightPath);
		mono_add_internal_call("Eagle.Navigation::FindSmoothPath_Native", Eagle::Script::Eagle_Navigation_FindSmoothPath);
		mono_add_internal_call("Eagle.Navigation::FindDistanceToWall_Native", Eagle::Script::Eagle_Navigation_FindDistanceToWall);
		mono_add_internal_call("Eagle.Navigation::FindRandomPoint_Native", Eagle::Script::Eagle_Navigation_FindRandomPoint);
		mono_add_internal_call("Eagle.Navigation::FindRandomPointInCircle_Native", Eagle::Script::Eagle_Navigation_FindRandomPointInCircle);
		mono_add_internal_call("Eagle.Navigation::IsValidPoint_Native", Eagle::Script::Eagle_Navigation_IsValidPoint);

		// CrowdNavigation
		mono_add_internal_call("Eagle.CrowdNavigation::SetMoveTarget_Native", Eagle::Script::Eagle_CrowdNavigation_SetMoveTarget);
		mono_add_internal_call("Eagle.CrowdNavigation::ResetMoveTarget_Native", Eagle::Script::Eagle_CrowdNavigation_ResetMoveTarget);

		// Script Component
		mono_add_internal_call("Eagle.ScriptComponent::SetScript_Native", Eagle::Script::Eagle_ScriptComponent_SetScript);
		mono_add_internal_call("Eagle.ScriptComponent::GetScriptType_Native", Eagle::Script::Eagle_ScriptComponent_GetScriptType);
		mono_add_internal_call("Eagle.ScriptComponent::GetInstance_Native", Eagle::Script::Eagle_ScriptComponent_GetInstance);

		// Asset
		mono_add_internal_call("Eagle.Asset::Get_Native", Eagle::Script::Eagle_Asset_Get);
		mono_add_internal_call("Eagle.Asset::GetPath_Native", Eagle::Script::Eagle_Asset_GetPath);
		mono_add_internal_call("Eagle.Asset::GetAssetType_Native", Eagle::Script::Eagle_Asset_GetAssetType);

		// AssetTexture2D
		mono_add_internal_call("Eagle.AssetTexture2D::SetAnisotropy_Native", Eagle::Script::Eagle_AssetTexture2D_SetAnisotropy);
		mono_add_internal_call("Eagle.AssetTexture2D::GetAnisotropy_Native", Eagle::Script::Eagle_AssetTexture2D_GetAnisotropy);
		mono_add_internal_call("Eagle.AssetTexture2D::SetFilterMode_Native", Eagle::Script::Eagle_AssetTexture2D_SetFilterMode);
		mono_add_internal_call("Eagle.AssetTexture2D::GetFilterMode_Native", Eagle::Script::Eagle_AssetTexture2D_GetFilterMode);
		mono_add_internal_call("Eagle.AssetTexture2D::SetAddressMode_Native", Eagle::Script::Eagle_AssetTexture2D_SetAddressMode);
		mono_add_internal_call("Eagle.AssetTexture2D::GetAddressMode_Native", Eagle::Script::Eagle_AssetTexture2D_GetAddressMode);
		mono_add_internal_call("Eagle.AssetTexture2D::SetMipsCount_Native", Eagle::Script::Eagle_AssetTexture2D_SetMipsCount);
		mono_add_internal_call("Eagle.AssetTexture2D::GetMipsCount_Native", Eagle::Script::Eagle_AssetTexture2D_GetMipsCount);
		mono_add_internal_call("Eagle.AssetTexture2D::SetFormat_Native", Eagle::Script::Eagle_AssetTexture2D_SetFormat);
		mono_add_internal_call("Eagle.AssetTexture2D::SetIsNormalMap_Native", Eagle::Script::Eagle_AssetTexture2D_SetIsNormalMap);
		mono_add_internal_call("Eagle.AssetTexture2D::SetCompression_Native", Eagle::Script::Eagle_AssetTexture2D_SetCompression);
		mono_add_internal_call("Eagle.AssetTexture2D::GetFormat_Native", Eagle::Script::Eagle_AssetTexture2D_GetFormat);
		mono_add_internal_call("Eagle.AssetTexture2D::IsNormalMap_Native", Eagle::Script::Eagle_AssetTexture2D_IsNormalMap);
		mono_add_internal_call("Eagle.AssetTexture2D::GetCompressionQuality_Native", Eagle::Script::Eagle_AssetTexture2D_GetCompressionQuality);

		// AssetTextureCube
		mono_add_internal_call("Eagle.AssetTextureCube::SetLayerSize_Native", Eagle::Script::Eagle_AssetTextureCube_SetLayerSize);
		mono_add_internal_call("Eagle.AssetTextureCube::SetPrefilterSize_Native", Eagle::Script::Eagle_AssetTextureCube_SetPrefilterSize);
		mono_add_internal_call("Eagle.AssetTextureCube::SetFormat_Native", Eagle::Script::Eagle_AssetTextureCube_SetFormat);
		mono_add_internal_call("Eagle.AssetTextureCube::GetLayerSize_Native", Eagle::Script::Eagle_AssetTextureCube_GetLayerSize);
		mono_add_internal_call("Eagle.AssetTextureCube::GetPrefilterSize_Native", Eagle::Script::Eagle_AssetTextureCube_GetPrefilterSize);
		mono_add_internal_call("Eagle.AssetTextureCube::GetFormat_Native", Eagle::Script::Eagle_AssetTextureCube_GetFormat);

		// AssetMaterial
		mono_add_internal_call("Eagle.AssetMaterial::GetMaterial_Native", Eagle::Script::Eagle_AssetMaterial_GetMaterial);
		mono_add_internal_call("Eagle.AssetMaterial::SetMaterial_Native", Eagle::Script::Eagle_AssetMaterial_SetMaterial);
		mono_add_internal_call("Eagle.AssetMaterial::Create_Native", Eagle::Script::Eagle_AssetMaterial_Create);
		mono_add_internal_call("Eagle.AssetMaterial::CreateFromAsset_Native", Eagle::Script::Eagle_AssetMaterial_CreateFromAsset);

		// AssetAudio
		mono_add_internal_call("Eagle.AssetAudio::GetVolume_Native", Eagle::Script::Eagle_AssetAudio_GetVolume);
		mono_add_internal_call("Eagle.AssetAudio::SetVolume_Native", Eagle::Script::Eagle_AssetAudio_SetVolume);
		mono_add_internal_call("Eagle.AssetAudio::GetPitch_Native", Eagle::Script::Eagle_AssetAudio_GetPitch);
		mono_add_internal_call("Eagle.AssetAudio::SetPitch_Native", Eagle::Script::Eagle_AssetAudio_SetPitch);
		mono_add_internal_call("Eagle.AssetAudio::SetSoundGroup_Native", Eagle::Script::Eagle_AssetAudio_SetSoundGroup);
		mono_add_internal_call("Eagle.AssetAudio::GetSoundGroup_Native", Eagle::Script::Eagle_AssetAudio_GetSoundGroup);
		mono_add_internal_call("Eagle.AssetAudio::SetPan_Native", Eagle::Script::Eagle_AssetAudio_SetPan);
		mono_add_internal_call("Eagle.AssetAudio::GetPan_Native", Eagle::Script::Eagle_AssetAudio_GetPan);

		// AssetPhysicsMaterial
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::SetStaticFriction_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_SetStaticFriction);
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::SetDynamicFriction_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_SetDynamicFriction);
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::SetBounciness_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_SetBounciness);
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::GetStaticFriction_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_GetStaticFriction);
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::GetDynamicFriction_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_GetDynamicFriction);
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::GetBounciness_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_GetBounciness);
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::Create_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_Create);
		mono_add_internal_call("Eagle.AssetPhysicsMaterial::CreateFromAsset_Native", Eagle::Script::Eagle_AssetPhysicsMaterial_CreateFromAsset);

		// AssetSoundGroup
		mono_add_internal_call("Eagle.AssetSoundGroup::Stop_Native", Eagle::Script::Eagle_AssetSoundGroup_Stop);
		mono_add_internal_call("Eagle.AssetSoundGroup::SetPaused_Native", Eagle::Script::Eagle_AssetSoundGroup_SetPaused);
		mono_add_internal_call("Eagle.AssetSoundGroup::SetVolume_Native", Eagle::Script::Eagle_AssetSoundGroup_SetVolume);
		mono_add_internal_call("Eagle.AssetSoundGroup::SetMuted_Native", Eagle::Script::Eagle_AssetSoundGroup_SetMuted);
		mono_add_internal_call("Eagle.AssetSoundGroup::SetPitch_Native", Eagle::Script::Eagle_AssetSoundGroup_SetPitch);
		mono_add_internal_call("Eagle.AssetSoundGroup::GetVolume_Native", Eagle::Script::Eagle_AssetSoundGroup_GetVolume);
		mono_add_internal_call("Eagle.AssetSoundGroup::GetPitch_Native", Eagle::Script::Eagle_AssetSoundGroup_GetPitch);
		mono_add_internal_call("Eagle.AssetSoundGroup::IsPaused_Native", Eagle::Script::Eagle_AssetSoundGroup_IsPaused);
		mono_add_internal_call("Eagle.AssetSoundGroup::IsMuted_Native", Eagle::Script::Eagle_AssetSoundGroup_IsMuted);

		// AssetAnimation
		mono_add_internal_call("Eagle.AssetAnimation::SetRootMotionMode_Native", Eagle::Script::Eagle_AssetAnimation_SetRootMotionMode);
		mono_add_internal_call("Eagle.AssetAnimation::GetDuration_Native", Eagle::Script::Eagle_AssetAnimation_GetDuration);
		mono_add_internal_call("Eagle.AssetAnimation::GetTicksPerSecond_Native", Eagle::Script::Eagle_AssetAnimation_GetTicksPerSecond);
		mono_add_internal_call("Eagle.AssetAnimation::AddAnimationEvent_Native", Eagle::Script::Eagle_AssetAnimation_AddAnimationEvent);
		mono_add_internal_call("Eagle.AssetAnimation::RemoveAnimationEvent_Native", Eagle::Script::Eagle_AssetAnimation_RemoveAnimationEvent);
		mono_add_internal_call("Eagle.AssetAnimation::HasAnimationEvent_Native", Eagle::Script::Eagle_AssetAnimation_HasAnimationEvent);

		// AssetParticleSystem
		mono_add_internal_call("Eagle.AssetParticleSystem::GetEmittersCount_Native", Eagle::Script::Eagle_AssetParticleSystem_GetEmittersCount);
		mono_add_internal_call("Eagle.AssetParticleSystem::RemoveEmitters_Native", Eagle::Script::Eagle_AssetParticleSystem_RemoveEmitters);
		mono_add_internal_call("Eagle.AssetParticleSystem::SetEmitters_Prepare_Native", Eagle::Script::Eagle_AssetParticleSystem_SetEmitters_Prepare);
		mono_add_internal_call("Eagle.AssetParticleSystem::SetEmitters_Finish_Native", Eagle::Script::Eagle_AssetParticleSystem_SetEmitters_Finish);
		mono_add_internal_call("Eagle.AssetParticleSystem::SetEmitter_Native", Eagle::Script::Eagle_AssetParticleSystem_SetEmitter);
		mono_add_internal_call("Eagle.AssetParticleSystem::GetEmitter_Native", Eagle::Script::Eagle_AssetParticleSystem_GetEmitter);
		mono_add_internal_call("Eagle.AssetParticleSystem::Create_Native", Eagle::Script::Eagle_AssetParticleSystem_Create);

		mono_add_internal_call("Eagle.AssetBehaviorGraph::CreateTaskManager_Native", Eagle::Script::Eagle_AssetBehaviorGraph_CreateTaskManager);

		// Math
		mono_add_internal_call("Eagle.Mathf::GetForwardVector_Native", Eagle::Script::Eagle_Math_GetForwardVector);
		mono_add_internal_call("Eagle.Mathf::GetUpVector_Native", Eagle::Script::Eagle_Math_GetUpVector);
		mono_add_internal_call("Eagle.Mathf::GetRightVector_Native", Eagle::Script::Eagle_Math_GetRightVector);
		mono_add_internal_call("Eagle.Mathf::GetDirectionToPixel_Native", Eagle::Script::Eagle_Math_GetDirectionToPixel);
		mono_add_internal_call("Eagle.Mathf::CalculateDirection_Native", Eagle::Script::Eagle_Math_CalculateDirection);
		mono_add_internal_call("Eagle.Mathf::SlerpQuat_Native", Eagle::Script::Eagle_Math_SlerpQuat);
		mono_add_internal_call("Eagle.Mathf::LookAt_Native", Eagle::Script::Eagle_Math_LookAt);
		mono_add_internal_call("Eagle.Mathf::LookAtY_Native", Eagle::Script::Eagle_Math_LookAtY);
		mono_add_internal_call("Eagle.Mathf::UpAt_Native", Eagle::Script::Eagle_Math_UpAt);

		// Quat
		mono_add_internal_call("Eagle.Quat::Mul_Native", Eagle::Script::Eagle_Quat_Mul);
		mono_add_internal_call("Eagle.Quat::EulerAngles_Native", Eagle::Script::Eagle_Quat_EulerAngles);
		mono_add_internal_call("Eagle.Quat::FromEulerAngles_Native", Eagle::Script::Eagle_Quat_FromEulerAngles);
	}
}
