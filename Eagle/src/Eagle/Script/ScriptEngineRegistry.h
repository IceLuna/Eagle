#pragma once

extern "C"
{
	typedef struct _MonoType MonoType;
}

namespace Eagle
{
	class Entity;
	class Scene;
	class SceneComponent;
	class LightComponent;
	class BaseColliderComponent;

	// Type-erased access to an engine component from C#.
	// Plain function pointers keep the code generated per component type small.
	struct ComponentBindings
	{
		bool (*Has)(Entity&) = nullptr;
		void (*Add)(Entity&) = nullptr;
		void (*Remove)(Entity&) = nullptr;
		std::vector<Entity> (*GetAllEntitiesWith)(const Ref<Scene>&) = nullptr;

		// Set only if the component derives from the corresponding base. Otherwise null
		SceneComponent* (*AsSceneComponent)(Entity&) = nullptr;
		LightComponent* (*AsLightComponent)(Entity&) = nullptr;
		BaseColliderComponent* (*AsColliderComponent)(Entity&) = nullptr;
	};

	class ScriptEngineRegistry
	{
	public:
		static void RegisterAll();

		// Returns null if `type` is not a registered engine component
		static const ComponentBindings* FindComponentBindings(MonoType* type);

		// Return null (and log an error) if `type` is not a registered component of the requested base
		static SceneComponent* GetSceneComponent(Entity& entity, MonoType* type);
		static LightComponent* GetLightComponent(Entity& entity, MonoType* type);
		static BaseColliderComponent* GetColliderComponent(Entity& entity, MonoType* type);

	private:
		static void BindFunctions();
	};
}
