#pragma once

namespace Eagle
{
	class ProjectFileAssociationWidget
	{
	public:
		// Must be called every frame, after the editor's windows were submitted.
		// The first call checks the association
		static void Draw();

		static void Open();
	};
}
