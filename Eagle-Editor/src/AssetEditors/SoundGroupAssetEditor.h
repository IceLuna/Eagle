#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"

namespace Eagle
{
	class AssetSoundGroup;

	class SoundGroupAssetEditor : public AssetEditor
	{
	public:
		SoundGroupAssetEditor(const Ref<AssetSoundGroup>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		static std::string DescribeState(const AssetSoundGroup::Settings& before, const AssetSoundGroup::Settings& after);

	private:
		Ref<AssetSoundGroup> m_Asset;
		std::string m_WindowName;
		AssetEditTracker<AssetSoundGroup, AssetSoundGroup::Settings> m_EditTracker;
	};
}
