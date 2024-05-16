#pragma once

namespace Eagle
{
	class Asset;

	class AssetEditor
	{
	public:
		virtual ~AssetEditor() = default;

		// pOpen - In case X button is clicked, this flag will be set to false.
		// pOpen - if nullptr set, windows will not have X button 
		virtual void OnImGuiRender(bool* pOpen) {}

		virtual void SetInFocus();

		virtual void OnEvent(Event& e) {}

		virtual const Ref<Asset> GetAsset() const = 0;
	};
}
