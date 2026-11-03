#include "AssetUndo.h"

namespace Eagle::UndoUtils
{
	std::string Describe(std::initializer_list<NamedChange> changes, std::string_view fallback)
	{
		const NamedChange* changed = nullptr;
		for (const NamedChange& change : changes)
		{
			if (!change.bChanged)
				continue;

			if (changed)
				return std::string(fallback); // Several

			changed = &change;
		}

		if (!changed)
			return std::string(fallback);

		return "Edit " + std::string(changed->Name);
	}
}
