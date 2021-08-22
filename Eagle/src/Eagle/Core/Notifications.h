#pragma once

#include <vector>
#include <entt.hpp>

namespace Eagle
{
	enum class Notification
	{
		None,
		OnParentTransformChanged
	};
	class Component;
	class Entity;

	class ComponentsNotificationSystem
	{
	public:
		static void AddObserver(const Entity& parent, Component* observer);
		static void RemoveObserver(const Entity& parent, Component* observer);
		static void Notify(const Entity& parent, Notification notification);
		static void ResetSystem() { m_Entities.clear();  }

	private:
		static std::unordered_map<entt::entity, std::vector<Component*>> m_Entities;
	};

	class Observer
	{
	public:
		Observer() = default;
		Observer(const Observer&) = default;
		Observer(Observer&&) = default;

		Observer& operator=(const Observer&) = default;
		Observer& operator=(Observer&&) = default;

		virtual ~Observer() = default;
		virtual void OnNotify(Notification notification) {}
	};

	class Subject
	{
	public:
		Subject() = default;
		Subject(const Subject&) = default;
		Subject(Subject&&) = default;

		Subject& operator=(const Subject&) = default;
		Subject& operator=(Subject&&) = default;

		virtual ~Subject() = default;

		void AddObserver(Observer* observer) { m_Observers.push_back(observer); }

		void RemoveObserver(Observer* observer)
		{
			auto it = std::find(m_Observers.begin(), m_Observers.end(), observer);
			if (it != m_Observers.end())
			{
				m_Observers.erase(it);
			}
		}

	protected:
		void notify(Notification notification)
		{
			for (auto& myObserver : m_Observers)
			{
				myObserver->OnNotify(notification);
			}
		}
	
	private:
		std::vector<Observer*> m_Observers;
	};
}
