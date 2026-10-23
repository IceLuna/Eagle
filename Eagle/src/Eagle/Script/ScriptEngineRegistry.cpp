#include "egpch.h"
#include "ScriptEngineRegistry.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Core/Entity.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>

namespace Eagle
{
	static ankerl::unordered_dense::map<MonoType*, ComponentBindings> s_ComponentBindings;

	template<typename T>
	static void RegisterComponentType(const char* csharpTypeName)
	{
		MonoType* type = mono_reflection_type_from_name((char*)csharpTypeName, ScriptEngine::GetCoreAssemblyImage());
		if (!type)
		{
			EG_CORE_ERROR("No C# Component found for {}!", csharpTypeName);
			return;
		}

		ComponentBindings& bindings = s_ComponentBindings[type];
		bindings.Has = [](Entity& entity) { return entity.HasComponent<T>(); };
		bindings.Add = [](Entity& entity) { entity.AddComponent<T>(); };
		bindings.Remove = [](Entity& entity) { entity.RemoveComponent<T>(); };
		bindings.GetAllEntitiesWith = [](const Ref<Scene>& scene) { return scene->GetAllEntitiesWith_Vector<T>(); };

		if constexpr (std::is_base_of_v<SceneComponent, T>)
			bindings.AsSceneComponent = [](Entity& entity) -> SceneComponent* { return &entity.GetComponent<T>(); };

		if constexpr (std::is_base_of_v<LightComponent, T>)
			bindings.AsLightComponent = [](Entity& entity) -> LightComponent* { return &entity.GetComponent<T>(); };

		if constexpr (std::is_base_of_v<BaseColliderComponent, T>)
			bindings.AsColliderComponent = [](Entity& entity) -> BaseColliderComponent* { return &entity.GetComponent<T>(); };
	}

#define REGISTER_COMPONENT_TYPE(Type) RegisterComponentType<Type>("Eagle." #Type)

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

	const ComponentBindings* ScriptEngineRegistry::FindComponentBindings(MonoType* type)
	{
		auto it = s_ComponentBindings.find(type);
		return it != s_ComponentBindings.end() ? &it->second : nullptr;
	}

	SceneComponent* ScriptEngineRegistry::GetSceneComponent(Entity& entity, MonoType* type)
	{
		const ComponentBindings* bindings = FindComponentBindings(type);
		if (bindings && bindings->AsSceneComponent)
			return bindings->AsSceneComponent(entity);

		EG_CORE_ERROR("[ScriptEngine] '{}' is not a SceneComponent", mono_type_get_name(type));
		return nullptr;
	}

	LightComponent* ScriptEngineRegistry::GetLightComponent(Entity& entity, MonoType* type)
	{
		const ComponentBindings* bindings = FindComponentBindings(type);
		if (bindings && bindings->AsLightComponent)
			return bindings->AsLightComponent(entity);

		EG_CORE_ERROR("[ScriptEngine] '{}' is not a LightComponent", mono_type_get_name(type));
		return nullptr;
	}

	BaseColliderComponent* ScriptEngineRegistry::GetColliderComponent(Entity& entity, MonoType* type)
	{
		const ComponentBindings* bindings = FindComponentBindings(type);
		if (bindings && bindings->AsColliderComponent)
			return bindings->AsColliderComponent(entity);

		EG_CORE_ERROR("[ScriptEngine] '{}' is not a collider component", mono_type_get_name(type));
		return nullptr;
	}
}
