#pragma once

#include "Eagle/Core.h"
#include "Eagle/Events/Event.h"
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	class EAGLE_API Layer
	{
	public:
		Layer(const std::string& name);
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep timestep) {}
		virtual void OnImGuiRender() {}

		virtual void OnEvent(Event& e) {}

		inline const std::string& GetName() const noexcept { return m_DebugName; }

	protected:
		std::string m_DebugName;
	};
}
