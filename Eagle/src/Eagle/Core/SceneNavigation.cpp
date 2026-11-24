#include "egpch.h"
#include "Scene.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Debug/CPUTimings.h"

namespace Eagle
{
	// A nav mesh keeps only the areas it uses (and Default), so the project's other areas don't make it outdated.
	// It is if its areas aren't in the project's order anymore, or one of them was removed
	static bool IsNavAreaPriorityOutdated(std::span<const uint32_t> meshPriority, std::span<const AINavigation::Area> projectAreas)
	{
		auto it = meshPriority.begin();
		for (const AINavigation::Area& area : projectAreas)
		{
			if (it != meshPriority.end() && *it == area.ID)
				++it;
		}
		return it != meshPriority.end();
	}

	std::vector<BaseColliderComponent*> Scene::GatherObstacleColliders()
	{
		std::vector<BaseColliderComponent*> colliders;
		colliders.reserve(100u);
		{
			auto view = m_Registry.view<BoxColliderComponent>();
			for (auto entity : view)
				colliders.push_back(&view.get<BoxColliderComponent>(entity));
		}
		{
			auto view = m_Registry.view<SphereColliderComponent>();
			for (auto entity : view)
				colliders.push_back(&view.get<SphereColliderComponent>(entity));
		}
		{
			auto view = m_Registry.view<CapsuleColliderComponent>();
			for (auto entity : view)
				colliders.push_back(&view.get<CapsuleColliderComponent>(entity));
		}
		return colliders;
	}

	uint32_t Scene::FindNavMeshSlot(const Ref<AINavigation::Mesh>& navMesh) const
	{
		if (!navMesh)
			return InvalidNavMeshSlot;

		for (uint32_t slot = 0; slot < (uint32_t)m_NavMeshSlots.size(); ++slot)
		{
			if (m_NavMeshSlots[slot].Mesh == navMesh)
				return slot;
		}

		return InvalidNavMeshSlot;
	}

	const Ref<AINavigation::Mesh>& Scene::GetNavMeshInSlot(uint32_t slot) const
	{
		static const Ref<AINavigation::Mesh> s_Null;
		return slot < (uint32_t)m_NavMeshSlots.size() ? m_NavMeshSlots[slot].Mesh : s_Null;
	}

	const Ref<AINavigation::Mesh>& Scene::GetNavMesh(uint32_t agentTypeID) const
	{
		for (const NavMeshSlot& slot : m_NavMeshSlots)
		{
			if (slot.Mesh && slot.Mesh->GetAgentType().ID == agentTypeID)
				return slot.Mesh;
		}

		static const Ref<AINavigation::Mesh> s_Null;
		return s_Null;
	}

	NavigationMeshComponent* Scene::GetActiveNavMeshComponent(uint32_t agentTypeID) const
	{
		auto it = std::find_if(m_ActiveNavMeshes.begin(), m_ActiveNavMeshes.end(), [agentTypeID](const ActiveNavMesh& active) { return active.AgentTypeID == agentTypeID; });
		if (it == m_ActiveNavMeshes.end())
			return nullptr;

		Entity entity = GetEntityByGUID(it->Entity);
		if (!entity || !entity.HasComponent<NavigationMeshComponent>())
			return nullptr;

		auto& component = entity.GetComponent<NavigationMeshComponent>();
		return component.GetAgentTypeID() == agentTypeID ? &component : nullptr;
	}

	void Scene::SetActiveNavMeshComponent(NavigationMeshComponent& navMesh)
	{
		const GUID guid = navMesh.Parent.GetGUID();
		auto it = std::find_if(m_ActiveNavMeshes.begin(), m_ActiveNavMeshes.end(), [&navMesh](const ActiveNavMesh& active) { return active.AgentTypeID == navMesh.GetAgentTypeID(); });
		if (it == m_ActiveNavMeshes.end())
		{
			m_ActiveNavMeshes.push_back({ guid, navMesh.GetAgentTypeID() });
			return;
		}

		if (it->Entity == guid)
			return;

		// Only an active component can have a nav mesh
		NavigationMeshComponent* previous = GetActiveNavMeshComponent(navMesh.GetAgentTypeID());
		it->Entity = guid;
		if (previous && previous->GetNavMesh())
		{
			DestroyNavMesh_Internal(*previous);
			RefreshCrowdAgents();
		}
	}

	void Scene::OnNavMeshAddedAgentType(NavigationMeshComponent& navMesh)
	{
		if (!Project::FindNavAgentType(navMesh.GetAgentTypeID()))
			return;

		const bool bRecorded = std::any_of(m_ActiveNavMeshes.begin(), m_ActiveNavMeshes.end(), [&navMesh](const ActiveNavMesh& active) { return active.AgentTypeID == navMesh.GetAgentTypeID(); });
		if (!bRecorded)
			m_ActiveNavMeshes.push_back({ navMesh.Parent.GetGUID(), navMesh.GetAgentTypeID() });
	}

	void Scene::OnNavMeshRemovedAgentType(NavigationMeshComponent& navMesh, uint32_t agentTypeID)
	{
		auto it = std::find_if(m_ActiveNavMeshes.begin(), m_ActiveNavMeshes.end(), [agentTypeID](const ActiveNavMesh& active) { return active.AgentTypeID == agentTypeID; });
		if (it == m_ActiveNavMeshes.end() || it->Entity != navMesh.Parent.GetGUID())
			return;

		if (const NavigationMeshComponent* next = FindNavMeshComponentToActivate(agentTypeID, &navMesh))
			it->Entity = next->Parent.GetGUID();
		else
			m_ActiveNavMeshes.erase(it);
	}

	const NavigationMeshComponent* Scene::FindNavMeshComponentToActivate(uint32_t agentTypeID, const NavigationMeshComponent* exclude) const
	{
		const NavigationMeshComponent* result = nullptr;
		auto view = m_Registry.view<NavigationMeshComponent>();
		for (auto entity : view)
		{
			const auto& component = view.get<NavigationMeshComponent>(entity);
			if (&component == exclude || component.GetAgentTypeID() != agentTypeID)
				continue;

			// The lowest GUID, so that the choice doesn't depend on the order of the components in the registry
			if (!result || component.Parent.GetGUID() < result->Parent.GetGUID())
				result = &component;
		}
		return result;
	}

	void Scene::ValidateActiveNavMeshes()
	{
		// Recorded components that don't exist, or that use another type now
		std::erase_if(m_ActiveNavMeshes, [this](const ActiveNavMesh& active)
		{
			return !Project::FindNavAgentType(active.AgentTypeID) || !GetActiveNavMeshComponent(active.AgentTypeID);
		});

		// Types that have components but no active one
		auto view = m_Registry.view<NavigationMeshComponent>();
		for (auto entity : view)
		{
			const uint32_t agentTypeID = view.get<NavigationMeshComponent>(entity).GetAgentTypeID();
			if (!Project::FindNavAgentType(agentTypeID) || GetActiveNavMeshComponent(agentTypeID))
				continue;

			m_ActiveNavMeshes.push_back({ FindNavMeshComponentToActivate(agentTypeID, nullptr)->Parent.GetGUID(), agentTypeID });
		}

		// Only active components can have nav meshes
		std::vector<NavigationMeshComponent*> builtInactive;
		for (auto entity : view)
		{
			auto& component = view.get<NavigationMeshComponent>(entity);
			if (component.GetNavMesh() && !component.IsActive())
				builtInactive.push_back(&component);
		}

		for (NavigationMeshComponent* component : builtInactive)
		{
			EG_CORE_ERROR("[AINavigation] The nav mesh of `{}` was built, but another nav mesh uses its agent type. It's destroyed", component->Parent.GetName());
			DestroyNavMesh_Internal(*component);
		}

		if (!builtInactive.empty())
			RefreshCrowdAgents();
	}

	uint32_t Scene::FindNavMeshSlotForAgent(uint32_t agentTypeID, float radius, float height) const
	{
		if (agentTypeID != AINavigation::AutoAgentTypeID)
		{
			for (uint32_t slot = 0; slot < (uint32_t)m_NavMeshSlots.size(); ++slot)
			{
				const auto& mesh = m_NavMeshSlots[slot].Mesh;
				if (mesh && mesh->GetAgentType().ID == agentTypeID)
					return slot;
			}
			return InvalidNavMeshSlot;
		}

		// Auto. The smallest agent type that fits. If none fits, the largest one
		auto IsSmaller = [](const AINavigation::AgentType& a, const AINavigation::AgentType& b)
		{
			return a.Radius < b.Radius || (a.Radius == b.Radius && a.Height < b.Height);
		};

		uint32_t bestFitting = InvalidNavMeshSlot;
		uint32_t largest = InvalidNavMeshSlot;
		for (uint32_t slot = 0; slot < (uint32_t)m_NavMeshSlots.size(); ++slot)
		{
			const auto& mesh = m_NavMeshSlots[slot].Mesh;
			if (!mesh)
				continue;

			const AINavigation::AgentType& type = mesh->GetAgentType();
			if (largest == InvalidNavMeshSlot || IsSmaller(m_NavMeshSlots[largest].Mesh->GetAgentType(), type))
				largest = slot;

			const bool bFits = type.Radius >= radius && type.Height >= height;
			if (bFits && (bestFitting == InvalidNavMeshSlot || IsSmaller(type, m_NavMeshSlots[bestFitting].Mesh->GetAgentType())))
				bestFitting = slot;
		}

		return bestFitting != InvalidNavMeshSlot ? bestFitting : largest;
	}

	uint32_t Scene::GetFallbackNavAgentTypeID(const NavigationMeshComponent* exclude) const
	{
		auto IsUsed = [this, exclude](uint32_t agentTypeID)
		{
			auto view = m_Registry.view<NavigationMeshComponent>();
			for (auto entity : view)
			{
				const auto& component = view.get<NavigationMeshComponent>(entity);
				if (&component != exclude && component.GetAgentTypeID() == agentTypeID)
					return true;
			}
			return false;
		};

		const uint32_t defaultID = Project::GetDefaultNavAgentTypeID();
		if (!IsUsed(defaultID))
			return defaultID;

		for (const AINavigation::AgentType& type : Project::GetNavAgentTypes())
		{
			if (!IsUsed(type.ID))
				return type.ID;
		}

		return defaultID;
	}

	void Scene::DestroyNavMesh_Internal(NavigationMeshComponent& navMesh)
	{
		const uint32_t slot = FindNavMeshSlot(navMesh.GetNavMesh());
		if (slot != InvalidNavMeshSlot)
		{
			auto agents = m_Registry.view<NavigationCrowdAgentComponent>();
			for (auto entity : agents)
			{
				auto& agent = agents.get<NavigationCrowdAgentComponent>(entity);
				if (agent.IsValid() && agent.GetNavMesh() == navMesh.GetNavMesh())
					agent.RemoveAgent();
			}

			// The obstacles are destroyed with the nav mesh. So the colliders just forget them.
			// Removing them one by one would queue a request per obstacle on a mesh that's about to be destroyed anyway,
			// and a failed removal could leave a stale ref that was later sent to a new nav mesh in this slot
			for (BaseColliderComponent* collider : GatherObstacleColliders())
				collider->ForgetObstacle(slot);

			m_NavMeshSlots[slot] = {};
		}

		navMesh.ReleaseNavMesh();
	}

	bool Scene::BuildNavMesh(NavigationMeshComponent& navMesh)
	{
		const AINavigation::AgentType* agentType = Project::FindNavAgentType(navMesh.GetAgentTypeID());
		if (!agentType)
		{
			EG_CORE_ERROR("[AINavigation] Failed to build the nav mesh of `{}`. Its agent type (ID: {}) doesn't exist", navMesh.Parent.GetName(), navMesh.GetAgentTypeID());
			return false;
		}

		if (!navMesh.IsActive())
		{
			const NavigationMeshComponent* active = GetActiveNavMeshComponent(navMesh.GetAgentTypeID());
			EG_CORE_ERROR("[AINavigation] Failed to build the nav mesh of `{}`. `{}` already uses the agent type `{}`. Pick another agent type",
				navMesh.Parent.GetName(), active ? active->Parent.GetName() : std::string(), agentType->Name);
			return false;
		}

		DestroyNavMesh_Internal(navMesh);
		navMesh.Build();
		if (!navMesh.GetNavMesh())
		{
			RefreshCrowdAgents(); // Agents that were on it might fit another nav mesh
			return false;
		}

		uint32_t freeSlot = InvalidNavMeshSlot;
		for (uint32_t slot = 0; slot < (uint32_t)m_NavMeshSlots.size(); ++slot)
		{
			const auto& slotMesh = m_NavMeshSlots[slot].Mesh;
			EG_CORE_ASSERT(!slotMesh || slotMesh->GetAgentType().ID != agentType->ID); // Only the active component of a type can be built
			if (!slotMesh && freeSlot == InvalidNavMeshSlot)
				freeSlot = slot;
		}

		// There's at most one active component per agent type, and at most `MaxAgentTypes` agent types. So there's always a free slot
		EG_CORE_ASSERT(freeSlot != InvalidNavMeshSlot);
		if (freeSlot == InvalidNavMeshSlot)
		{
			EG_CORE_ERROR("[AINavigation] Failed to build the nav mesh of `{}`. The scene already has {} nav meshes", navMesh.Parent.GetName(), AINavigation::MaxAgentTypes);
			navMesh.ReleaseNavMesh();
			return false;
		}

		m_NavMeshSlots[freeSlot] = { navMesh.Parent.GetGUID(), navMesh.GetNavMesh() };

		for (BaseColliderComponent* collider : GatherObstacleColliders())
			collider->RecreateObstacle(freeSlot);

		RefreshCrowdAgents();
		return true;
	}

	void Scene::DestroyNavMesh(NavigationMeshComponent& navMesh)
	{
		const bool bBuilt = navMesh.GetNavMesh() != nullptr;
		if (bBuilt)
			DestroyNavMesh_Internal(navMesh);

		navMesh.DestroyNavMesh();

		if (bBuilt)
			RefreshCrowdAgents();
	}

	void Scene::RebuildNavMeshes()
	{
		// Copied, since rebuilding changes the slots
		std::vector<GUID> builtEntities;
		for (const NavMeshSlot& slot : m_NavMeshSlots)
		{
			if (slot.Mesh)
				builtEntities.push_back(slot.Entity);
		}

		for (const GUID& guid : builtEntities)
		{
			Entity entity = GetEntityByGUID(guid);
			if (!entity || !entity.HasComponent<NavigationMeshComponent>())
				continue;

			auto& component = entity.GetComponent<NavigationMeshComponent>();
			if (component.bAutoRebuild)
				BuildNavMesh(component);
		}
	}

	void Scene::RequestNavMeshRebuild(const std::optional<AABB>& region)
	{
		bool bTouches = false;
		for (const NavMeshSlot& slot : m_NavMeshSlots)
		{
			if (!slot.Mesh)
				continue;

			const AABB& bounds = slot.Mesh->GetAABB();
			if (!region || (glm::all(glm::lessThanEqual(region->Min, bounds.Max)) && glm::all(glm::lessThanEqual(bounds.Min, region->Max))))
			{
				bTouches = true;
				break;
			}
		}

		if (!bTouches)
			return;

		if (region)
			m_NavMeshRebuildRegions.push_back(*region);
		else
			m_bRebuildAllNavMeshes = true;
	}

	void Scene::ProcessNavMeshRebuildRequests()
	{
		if (!m_bRebuildAllNavMeshes && m_NavMeshRebuildRegions.empty())
			return;

		const bool bAll = m_bRebuildAllNavMeshes;
		const std::vector<AABB> regions = std::move(m_NavMeshRebuildRegions);
		m_bRebuildAllNavMeshes = false;

		// Copied, since rebuilding changes the slots
		std::vector<GUID> toRebuild;
		for (const NavMeshSlot& slot : m_NavMeshSlots)
		{
			if (!slot.Mesh)
				continue;

			const AABB& bounds = slot.Mesh->GetAABB();
			const bool bTouched = bAll || std::ranges::any_of(regions, [&bounds](const AABB& region)
			{
				return glm::all(glm::lessThanEqual(region.Min, bounds.Max)) && glm::all(glm::lessThanEqual(bounds.Min, region.Max));
			});
			if (bTouched)
				toRebuild.push_back(slot.Entity);
		}

		for (const GUID& guid : toRebuild)
		{
			Entity entity = GetEntityByGUID(guid);
			if (!entity || !entity.HasComponent<NavigationMeshComponent>())
				continue;

			auto& component = entity.GetComponent<NavigationMeshComponent>();
			if (component.bAutoRebuild)
				BuildNavMesh(component);
		}
	}

	AINavigation::AreaCostSources Scene::GetNavAreaCostSources(uint32_t agentTypeID) const
	{
		AINavigation::AreaCostSources sources;
		sources.Areas = Project::GetNavAreas();
		if (const AINavigation::AgentType* type = Project::FindNavAgentType(agentTypeID))
			sources.AgentTypeOverrides = type->AreaOverrides;

		auto it = m_NavBlockedAreas.find(agentTypeID);
		if (it != m_NavBlockedAreas.end())
			sources.BlockedAreas = it->second;

		return sources;
	}

	void Scene::SetNavAreaBlocked(uint32_t agentTypeID, uint32_t areaID, bool bBlocked)
	{
		auto& blocks = m_NavBlockedAreas[agentTypeID];
		auto it = std::ranges::find_if(blocks, [areaID](const AINavigation::AreaBlock& block) { return block.AreaID == areaID; });
		if (it == blocks.end())
			blocks.push_back({ areaID, bBlocked });
		else
			it->bBlocked = bBlocked;

		if (const auto& mesh = GetNavMesh(agentTypeID))
			mesh->SetAreaCosts(GetNavAreaCostSources(agentTypeID));
	}

	void Scene::ResetNavAreaBlocked(uint32_t agentTypeID, uint32_t areaID)
	{
		auto it = m_NavBlockedAreas.find(agentTypeID);
		if (it == m_NavBlockedAreas.end())
			return;

		if (std::erase_if(it->second, [areaID](const AINavigation::AreaBlock& block) { return block.AreaID == areaID; }) == 0u)
			return;

		if (const auto& mesh = GetNavMesh(agentTypeID))
			mesh->SetAreaCosts(GetNavAreaCostSources(agentTypeID));
	}

	void Scene::ResetNavAreasBlocked(uint32_t agentTypeID)
	{
		if (m_NavBlockedAreas.erase(agentTypeID) == 0u)
			return;

		if (const auto& mesh = GetNavMesh(agentTypeID))
			mesh->SetAreaCosts(GetNavAreaCostSources(agentTypeID));
	}

	bool Scene::IsNavAreaBlocked(uint32_t agentTypeID, uint32_t areaID) const
	{
		const AINavigation::AreaCostSources sources = GetNavAreaCostSources(agentTypeID);
		const AINavigation::Area* area = AINavigation::FindAreaOrDefault(sources.Areas, areaID);
		return area && AINavigation::ResolveAreaBlocked(*area, sources);
	}

	void Scene::UpdateNavMeshAreaCosts()
	{
		for (const NavMeshSlot& slot : m_NavMeshSlots)
		{
			if (slot.Mesh)
				slot.Mesh->SetAreaCosts(GetNavAreaCostSources(slot.Mesh->GetAgentType().ID));
		}
	}

	void Scene::OnNavAreasChanged()
	{
		const auto& areas = Project::GetNavAreas();

		std::vector<GUID> outdated;
		for (const NavMeshSlot& slot : m_NavMeshSlots)
		{
			if (slot.Mesh && slot.Mesh->HasAreas() && IsNavAreaPriorityOutdated(slot.Mesh->GetAreaIDsByPriority(), areas))
				outdated.push_back(slot.Entity);
		}

		for (const GUID& guid : outdated)
		{
			Entity entity = GetEntityByGUID(guid);
			if (!entity || !entity.HasComponent<NavigationMeshComponent>())
				continue;

			auto& component = entity.GetComponent<NavigationMeshComponent>();
			if (component.bAutoRebuild)
				BuildNavMesh(component);
		}

		UpdateNavMeshAreaCosts();
	}

	void Scene::RebuildCrowd(NavigationMeshComponent& navMesh)
	{
		const Ref<AINavigation::Mesh>& mesh = navMesh.GetNavMesh();
		if (FindNavMeshSlot(mesh) == InvalidNavMeshSlot)
			return;

		auto agents = m_Registry.view<NavigationCrowdAgentComponent>();
		for (auto entity : agents)
		{
			auto& agent = agents.get<NavigationCrowdAgentComponent>(entity);
			if (agent.IsValid() && agent.GetNavMesh() == mesh)
				agent.RemoveAgent();
		}

		mesh->GetCrowd().SetSettings(navMesh.GetCrowdSettings());
		RefreshCrowdAgents();
	}

	void Scene::RefreshCrowdAgents()
	{
		auto agents = m_Registry.view<NavigationCrowdAgentComponent>();
		for (auto entity : agents)
		{
			auto& agent = agents.get<NavigationCrowdAgentComponent>(entity);
			const auto& settings = agent.GetSettings();
			const uint32_t slot = FindNavMeshSlotForAgent(agent.GetAgentTypeID(), settings.AgentRadius, settings.AgentHeight);
			if (agent.IsValid() && agent.GetNavMesh() == GetNavMeshInSlot(slot))
				continue;

			agent.RemoveAgent();
			agent.CreateAgent(agent.Parent.GetWorldLocation());
		}
	}

	void Scene::OnNavMeshAgentTypeChanged(NavigationMeshComponent& navMesh, uint32_t oldAgentTypeID)
	{
		// The slot is found by the nav mesh itself, so it doesn't matter that the component already has the new type
		const bool bWasBuilt = navMesh.GetNavMesh() != nullptr;
		if (bWasBuilt)
			DestroyNavMesh_Internal(navMesh);

		OnNavMeshRemovedAgentType(navMesh, oldAgentTypeID);
		OnNavMeshAddedAgentType(navMesh);

		if (bWasBuilt && navMesh.IsActive())
			BuildNavMesh(navMesh);
		else if (bWasBuilt)
			RefreshCrowdAgents();
	}

	void Scene::OnNavMeshComponentChanged(NavigationMeshComponent& navMesh)
	{
		if (navMesh.ShouldAutoRebuild() && navMesh.IsActive())
			BuildNavMesh(navMesh);
	}

	bool Scene::OnNavAgentTypesChanged()
	{
		bool bChanged = false;

		// Nav mesh components whose agent type was removed
		{
			std::vector<NavigationMeshComponent*> invalidComponents;
			auto view = m_Registry.view<NavigationMeshComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<NavigationMeshComponent>(entity);
				if (!Project::FindNavAgentType(component.GetAgentTypeID()))
					invalidComponents.push_back(&component);
			}

			for (NavigationMeshComponent* component : invalidComponents)
			{
				component->SetAgentTypeIDOrFallback(component->GetAgentTypeID());
				bChanged = true;
			}

			ValidateActiveNavMeshes();
		}

		// Built nav meshes whose agent type has changed since they were built
		{
			std::vector<GUID> outdated;
			for (const NavMeshSlot& slot : m_NavMeshSlots)
			{
				if (!slot.Mesh)
					continue;

				const AINavigation::AgentType* type = Project::FindNavAgentType(slot.Mesh->GetAgentType().ID);
				if (type && !type->HasSameDimensions(slot.Mesh->GetAgentType()))
					outdated.push_back(slot.Entity);
			}

			for (const GUID& guid : outdated)
			{
				Entity entity = GetEntityByGUID(guid);
				if (!entity || !entity.HasComponent<NavigationMeshComponent>())
					continue;

				auto& component = entity.GetComponent<NavigationMeshComponent>();
				if (component.bAutoRebuild)
					BuildNavMesh(component);
			}
		}

		// Crowd agents and script fields keep removed agent types, so that they don't move and it's noticed.
		// The refresh removes the agents of removed types from the crowds
		RefreshCrowdAgents();
		UpdateNavMeshAreaCosts();

		return bChanged;
	}

	std::vector<NavigationMeshComponent*> Scene::GetActiveNavMeshComponents() const
	{
		std::vector<NavigationMeshComponent*> result;
		result.reserve(m_ActiveNavMeshes.size());
		for (const ActiveNavMesh& active : m_ActiveNavMeshes)
		{
			if (NavigationMeshComponent* component = GetActiveNavMeshComponent(active.AgentTypeID))
				result.push_back(component);
		}

		// Sorted, so that the order doesn't depend on the order in which the types were used. For example, saving the same scene twice gives the same file
		std::sort(result.begin(), result.end(), [](const NavigationMeshComponent* a, const NavigationMeshComponent* b) { return a->GetAgentTypeID() < b->GetAgentTypeID(); });
		return result;
	}

	void Scene::UpdateNavMesh(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update NavMesh");
		for (const NavMeshSlot& slot : m_NavMeshSlots)
		{
			if (slot.Mesh)
				slot.Mesh->Update(ts);
		}
	}

	void Scene::MoveCrowdAgentEntities(Timestep ts)
	{
		auto view = m_Registry.view<NavigationCrowdAgentComponent>();
		for (auto e : view)
		{
			auto& agent = view.get<NavigationCrowdAgentComponent>(e);
			if (agent.IsValid())
				agent.MoveEntity(ts);
		}
	}

	void Scene::SyncCrowdAgentsFromEntities()
	{
		auto view = m_Registry.view<NavigationCrowdAgentComponent>();
		for (auto e : view)
		{
			auto& agent = view.get<NavigationCrowdAgentComponent>(e);
			if (agent.IsValid())
				agent.SyncFromEntity();
		}
	}

	void Scene::OnNavMeshAdded(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		OnNavMeshAddedAgentType(entity.GetComponent<NavigationMeshComponent>());
	}

	void Scene::OnNavMeshRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& component = entity.GetComponent<NavigationMeshComponent>();
		const bool bBuilt = FindNavMeshSlot(component.GetNavMesh()) != InvalidNavMeshSlot;
		if (bBuilt)
			DestroyNavMesh_Internal(component);
		else
			component.ReleaseNavMesh(); // For example, the scene is being cleared

		OnNavMeshRemovedAgentType(component, component.GetAgentTypeID());

		if (bBuilt)
			RefreshCrowdAgents();
	}

	void Scene::OnBoxColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<BoxColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnSphereColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<SphereColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnCapsuleColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<CapsuleColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnNavAreaVolumeRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		const auto& volume = entity.GetComponent<NavigationAreaVolumeComponent>();
		const auto& lastBounds = volume.GetLastBuildBounds();
		RequestNavMeshRebuild(lastBounds ? *lastBounds : volume.GetWorldBounds());
	}

	void Scene::OnCrowdAgentAdded(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
		component.CreateAgent(entity.GetWorldLocation());
	}

	void Scene::OnCrowdAgentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
		component.RemoveAgent();
	}
}
