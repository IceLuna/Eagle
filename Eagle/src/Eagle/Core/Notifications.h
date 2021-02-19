#pragma once

#include <vector>

namespace Eagle
{
	enum class Notification
	{
		None,
		OnParentTransformChanged
	};

	class Observer
	{
	public:
		virtual ~Observer() = default;
		virtual void OnNotify(Notification notification) {}
	};

	class Subject
	{
	public:
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
