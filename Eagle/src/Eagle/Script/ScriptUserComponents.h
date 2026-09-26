#pragma once

#include "ScriptUtils.h"

namespace Eagle
{
	// Runtime storage for C# components declared inside user app assembly.
	// It lives in the scene registry so its lifetime is tied to the entity and the scene:
	// destroying the entity, clearing the registry or destroying the (runtime) scene frees the GC handles.
	// It is intentionally NOT derived from `Eagle::Component` and it is NOT listed in:
	//   - Scene copy (`Scene::Scene(const Ref<Scene>&)` / `CopyComponents`)
	//   - SceneSerializer
	//   - Editor panels
	// so it never leaves the running scene and the editor never sees it.
	struct ScriptUserComponents
	{
		ScriptUserComponents() = default;
		ScriptUserComponents(const ScriptUserComponents&) = delete;
		ScriptUserComponents& operator=(const ScriptUserComponents&) = delete;
		ScriptUserComponents(ScriptUserComponents&&) = default;
		ScriptUserComponents& operator=(ScriptUserComponents&&) = default;

		// Key: exact C# class of the component.
		// Value: GC handle to the managed instance.
		ankerl::unordered_dense::map<MonoClass*, Scope<MonoInstance>> Instances;
	};
}
