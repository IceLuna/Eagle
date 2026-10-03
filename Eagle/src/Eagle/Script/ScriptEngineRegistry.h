#pragma once

namespace Eagle
{
	class ScriptEngineRegistry
	{
	public:
		static void RegisterAll();

	private:
		static void BindFunctions();
	};
}