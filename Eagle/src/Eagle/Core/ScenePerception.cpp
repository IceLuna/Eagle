#include "egpch.h"
#include "Scene.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Core/Random.h"
#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Script/ScriptEngine.h"

namespace Eagle
{
	static void GatherSelfAndChildren(const Entity& entity, std::set<GUID>& outEntities)
	{
		outEntities.insert(entity.GetGUID());
		for (const Entity& child : entity.GetChildren())
			GatherSelfAndChildren(child, outEntities);
	}

	static bool IsSelfOrChildOf(Entity entity, const Entity& parent)
	{
		while (entity)
		{
			if (entity == parent)
				return true;

			entity = entity.GetParent();
		}

		return false;
	}

	static Entity GetScopeTop(const Entity& entity, AIPerception::HierarchyScope scope)
	{
		if (scope == AIPerception::HierarchyScope::PhysicsBody)
		{
			for (Entity current = entity; current; current = current.GetParent())
			{
				if (current.HasAny<RigidBodyComponent, CharacterControllerComponent>())
					return current;
			}
		}

		return entity;
	}

	// Whether `candidate` is in the scope of `entity`
	static bool IsInScope(const Entity& candidate, const Entity& entity, AIPerception::HierarchyScope scope)
	{
		const Entity top = GetScopeTop(entity, scope);
		return scope == AIPerception::HierarchyScope::Self ? candidate == top : IsSelfOrChildOf(candidate, top);
	}

	static void GatherScope(const Entity& entity, AIPerception::HierarchyScope scope, std::set<GUID>& outEntities)
	{
		const Entity top = GetScopeTop(entity, scope);
		if (scope == AIPerception::HierarchyScope::Self)
			outEntities.insert(top.GetGUID());
		else
			GatherSelfAndChildren(top, outEntities);
	}

	// Whether nothing but the target is between the eye and the point
	static bool IsInLineOfSight(const PhysicsScene& physics, const glm::vec3& eye, const glm::vec3& point, const AIPerceptionSourceComponent& target, CollisionGroup traceGroup, const std::set<GUID>& ignored)
	{
		const glm::vec3 toPoint = point - eye;
		const float distance = glm::length(toPoint);
		if (distance < 0.0001f)
			return true;

		RaycastHit hit{};
		constexpr PhysicsQueryType query = PhysicsQueryType::Static | PhysicsQueryType::Dynamic;
		if (!physics.Raycast(eye, toPoint / distance, distance, query, traceGroup, &hit, &ignored))
			return true;

		return hit.HitEntity && IsInScope(hit.HitEntity, target.Parent, target.GetSettings().SeenWhenRayHits);
	}

	template<typename T>
	static void AgeDebugMarkers(std::vector<T>& markers, float dt)
	{
		for (T& marker : markers)
			marker.Age += dt;

		std::erase_if(markers, [](const T& marker) { return marker.Age > AIPerception::DebugMarkerLifetime; });
	}

	void Scene::UpdatePerception(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update AI Perception");

		const bool bDebugDraw = PerceptionDebugDraw != AIPerception::DebugDrawMode::Off;
		if (bDebugDraw)
			AgeDebugMarkers(m_PerceptionDebugNoises, m_bPaused ? 0.f : float(ts) * m_TimeScale);
		else
			m_PerceptionDebugNoises.clear();

		auto perceivers = m_Registry.view<AIPerceptionComponent>();
		if (perceivers.empty())
			return;

		auto sources = m_Registry.view<AIPerceptionSourceComponent>();
		std::vector<AIPerception::Memory::SightResult> seenTargets;
		std::set<GUID> ignored;

		for (auto e : perceivers)
		{
			auto& perception = perceivers.get<AIPerceptionComponent>(e);
			const AIPerception::Settings& settings = perception.GetSettings();
			const Entity entity(e, this);
			const float dt = GetEntityDeltaTime(entity, ts);
			auto& memory = perception.Memory;

			memory.ForgetIf([this](const GUID& target) { return !target.IsNull() && !GetEntityByGUID(target); });

			// Sight
			bool bCheckSight = false;
			if (settings.bSight)
			{
				// The first check is at a random time within the interval, so that AIs don't check at the same time
				if (perception.SightTimer < 0.f)
					perception.SightTimer = settings.SightInterval > 0.f ? Random::Float(0.f, settings.SightInterval) : 0.f;

				perception.SightTimer -= dt;
				if (perception.SightTimer <= 0.f)
				{
					perception.SightTimer = glm::max(perception.SightTimer + settings.SightInterval, 0.f);
					bCheckSight = true;
				}
			}
			else
			{
				perception.SightTimer = -1.f;
			}

			if (bCheckSight || !settings.bSight)
			{
				seenTargets.clear();
				if (bCheckSight && m_PhysicsScene)
				{
					const Transform eye = perception.GetEyeTransform();
					const glm::vec3 forward = Math::GetForwardVector(eye.Rotation);

					ignored.clear();
					GatherScope(entity, settings.IgnoreOwnBody, ignored);

					for (auto s : sources)
					{
						const auto& source = sources.get<AIPerceptionSourceComponent>(s);
						if (!source.GetSettings().bSight || ignored.contains(source.Parent.GetGUID()))
							continue;

						const GUID& targetID = source.Parent.GetGUID();
						const AIPerception::KnownTarget* known = memory.Find(targetID);
						const bool bWasSeen = known && known->IsSeen();
						const glm::vec3 point = source.GetSightPoint();

						if (AIPerception::CheckSight(settings, eye.Location, forward, point, bWasSeen) != AIPerception::SightCheck::Visible)
							continue;

						if (IsInLineOfSight(*m_PhysicsScene, eye.Location, point, source, settings.SightBlockedBy, ignored))
							seenTargets.push_back({ targetID, point });
					}
				}

				memory.UpdateSight(seenTargets, perception.PendingEvents);
			}

			memory.Update(dt, settings, perception.PendingEvents);

			if (bDebugDraw)
				AgeDebugMarkers(perception.DebugMarkers, dt);
			else
				perception.DebugMarkers.clear();
		}
	}

	void Scene::SendPerceptionEvents()
	{
		// Gathered first, since scripts can change the components
		struct PendingEvents
		{
			GUID Entity;
			std::vector<AIPerception::Event> Events;
		};
		std::vector<PendingEvents> pending;

		const bool bDebugDraw = PerceptionDebugDraw != AIPerception::DebugDrawMode::Off;
		auto perceivers = m_Registry.view<AIPerceptionComponent>();
		for (auto e : perceivers)
		{
			auto& perception = perceivers.get<AIPerceptionComponent>(e);
			if (perception.PendingEvents.empty())
				continue;

			if (bDebugDraw)
			{
				for (const AIPerception::Event& event : perception.PendingEvents)
				{
					if (event.bForgotten)
						continue;

					AIPerception::DebugMarker& marker = perception.DebugMarkers.emplace_back();
					marker.Location = event.Data.Location;
					marker.Type = event.Data.Type;
					marker.bSensed = event.Data.bSensed;
				}

				auto& markers = perception.DebugMarkers;
				if (markers.size() > AIPerception::MaxDebugMarkers)
					markers.erase(markers.begin(), markers.end() - AIPerception::MaxDebugMarkers);
			}

			pending.push_back({ perception.Parent.GetGUID(), std::move(perception.PendingEvents) });
		}

		for (const PendingEvents& entityEvents : pending)
		{
			for (const AIPerception::Event& event : entityEvents.Events)
			{
				// Looked up for each event since a script can destroy them
				const Entity entity = GetEntityByGUID(entityEvents.Entity);
				const Entity target = GetEntityByGUID(event.Target);
				if (!entity || (!target && !event.Target.IsNull()))
					continue;

				if (event.bForgotten)
					ScriptEngine::OnPerceptionForgotten(entity, target);
				else
					ScriptEngine::OnPerceptionUpdated(entity, target, event.Data);
			}
		}
	}

	void Scene::ReportNoise(const glm::vec3& location, float loudness, Entity instigator)
	{
		if (PerceptionDebugDraw != AIPerception::DebugDrawMode::Off)
		{
			if (m_PerceptionDebugNoises.size() >= AIPerception::MaxDebugNoises)
				m_PerceptionDebugNoises.erase(m_PerceptionDebugNoises.begin()); // The oldest

			AIPerception::DebugNoise& noise = m_PerceptionDebugNoises.emplace_back();
			noise.Location = location;
			noise.Loudness = loudness;
		}

		const GUID target = instigator ? instigator.GetGUID() : GUID(0, 0);
		auto perceivers = m_Registry.view<AIPerceptionComponent>();
		for (auto e : perceivers)
		{
			auto& perception = perceivers.get<AIPerceptionComponent>(e);
			const auto& settings = perception.GetSettings();

			// Ignore what its own body makes
			if (!settings.bHearing || (instigator && IsInScope(instigator, perception.Parent, settings.IgnoreOwnBody)))
				continue;

			if (AIPerception::CanHear(settings, perception.GetWorldTransform().Location, location, loudness))
				perception.Memory.AddStimulus(target, AIPerception::Sense::Hearing, location, loudness, perception.PendingEvents);
		}
	}

	void Scene::ReportDamage(Entity victim, Entity instigator, float amount, const std::optional<glm::vec3>& cameFrom)
	{
		if (!victim || !victim.HasComponent<AIPerceptionComponent>())
			return;

		// Ignore what its own body causes
		auto& perception = victim.GetComponent<AIPerceptionComponent>();
		const auto& settings = perception.GetSettings();
		if (!settings.bDamage || (instigator && IsInScope(instigator, victim, settings.IgnoreOwnBody)))
			return;

		const GUID target = instigator ? instigator.GetGUID() : GUID(0, 0);
		const glm::vec3 location = cameFrom ? *cameFrom : instigator ? instigator.GetWorldLocation() : victim.GetWorldLocation();
		perception.Memory.AddStimulus(target, AIPerception::Sense::Damage, location, amount, perception.PendingEvents);
	}
}
