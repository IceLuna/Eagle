#include "egpch.h"

#include "SceneComponent.h"
#include "Eagle/Core/Entity.h"
#include "Components.h"

namespace Eagle
{
	void Component::AddTag(const std::string& tag)
	{
		m_Tags.insert(tag);
	}

	void Component::RemoveTag(const std::string& tag)
	{
		auto& it = m_Tags.find(tag);
		if (it != m_Tags.end())
		{
			m_Tags.erase(it);
		}
	}

	void SceneComponent::OnInit(Entity& entity)
	{
		Component::OnInit(entity);

		auto& tc = entity.GetComponent<TransformComponent>();

		WorldTransform = tc.WorldTransform;
	}

	void SceneComponent::SetWorldTransform(const Transform& worldTransform)
	{
		auto& ownerWorldTransform = Owner.GetComponent<TransformComponent>().WorldTransform;
		WorldTransform = worldTransform;

		RelativeTransform.Translation = WorldTransform.Translation - ownerWorldTransform.Translation;
		RelativeTransform.Rotation = WorldTransform.Rotation - ownerWorldTransform.Rotation; //TODO: Figure out rotation calculation
		RelativeTransform.Scale3D = WorldTransform.Scale3D / ownerWorldTransform.Scale3D;
	}

	void SceneComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		auto& ownerWorldTransform = Owner.GetComponent<TransformComponent>().WorldTransform;
		RelativeTransform = relativeTransform;

		WorldTransform.Translation = ownerWorldTransform.Translation + RelativeTransform.Translation;
		WorldTransform.Rotation = ownerWorldTransform.Rotation + RelativeTransform.Rotation; //TODO: Figure out rotation calculation
		WorldTransform.Scale3D = ownerWorldTransform.Scale3D * RelativeTransform.Scale3D;
	}

	void SceneComponent::UpdateTransform()
	{
		SetRelativeTransform(GetRelativeTransform());
	}
}