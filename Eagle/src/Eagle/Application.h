#pragma once

#include "Core.h"

namespace Eagle
{
	class EAGLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	};

	//To be defined in CLIENT
	Application* CreateApplication();
}

