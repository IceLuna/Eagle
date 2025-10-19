#pragma once

#include "Eagle/Script/PublicField.h"

#include <string>
#include <map>

namespace Eagle
{
	// Represents the data of an AI class from C# (AITask, AINodeDecorator, AICompositeNode)
	struct AIBehaviorClassData
	{
		enum class ClassType
		{
			Unknown, Task, Composite, Decorator
		} Type = ClassType::Unknown;


		// We somehow need to associate UI node with the this behavior node.
		// We can't use name, or fields, because different nodes can have the same data.
		// This ID is genereted by the UI Behavior Graph when the node is created
		GUID ID = GUID(0, 0);

		std::string FullName; // Namespace.Name
		std::string Name;
		std::map<std::string, PublicField> Fields;
		bool bUserClass = false; // Controlled by ScriptEngine. Should not be modified by other code

		bool operator== (const AIBehaviorClassData& other) const
		{
			return FullName == other.FullName;
		}

		bool operator!= (const AIBehaviorClassData& other) const
		{
			return !(*this == other);
		}
	};

	struct AIBehaviorClasses
	{
		std::vector<AIBehaviorClassData> Tasks;
		std::vector<AIBehaviorClassData> Decorators;
		std::vector<AIBehaviorClassData> Composites;

		void Clear()
		{
			Tasks.clear();
			Decorators.clear();
			Composites.clear();
		}
	};

	struct AIBehaviorNode
	{
		AIBehaviorClassData Data;
		std::vector<AIBehaviorNode> Children;
		std::vector<AIBehaviorClassData> AttachedDecorators;
		size_t OrderIndex = 0; // Used to be displayed in UI
	};
}
