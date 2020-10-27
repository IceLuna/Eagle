#pragma once

#include "Eagle/Core.h"
#include "Eagle/Events/Event.h"

namespace Eagle
{
	class EAGLE_API Layer
	{
	public:
		Layer(const std::string& name);
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate() {}
		virtual void OnEvent(Event& e) {}

		inline const std::string& GetName() const noexcept { return m_DebugName; }

	protected:
		std::string m_DebugName;
	};
}
