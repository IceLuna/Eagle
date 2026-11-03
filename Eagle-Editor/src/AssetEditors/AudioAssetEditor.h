#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"

namespace Eagle
{
	class AssetAudio;

	class AudioAssetEditor : public AssetEditor
	{
	public:
		AudioAssetEditor(const Ref<AssetAudio>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		static std::string DescribeState(const AssetAudio::Settings& before, const AssetAudio::Settings& after);

	private:
		Ref<AssetAudio> m_Asset;
		std::string m_WindowName;
		AssetEditTracker<AssetAudio, AssetAudio::Settings> m_EditTracker;
	};
}
