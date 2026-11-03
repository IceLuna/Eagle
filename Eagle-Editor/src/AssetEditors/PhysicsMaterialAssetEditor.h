#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"

#include "Eagle/Core/Entity.h"

namespace Eagle
{
	class AssetPhysicsMaterial;
	class SphereColliderComponent;

	class PhysicsMaterialAssetEditor : public AssetEditor
	{
	public:
		PhysicsMaterialAssetEditor(const Ref<AssetPhysicsMaterial>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		void ResetScene();
		void WakeUpActors();

		static std::string DescribeState(const AssetPhysicsMaterial::Settings& before, const AssetPhysicsMaterial::Settings& after);

	private:
		Ref<AssetPhysicsMaterial> m_Asset;
		SphereColliderComponent* m_Component = nullptr; // Not owning
		AssetEditTracker<AssetPhysicsMaterial, AssetPhysicsMaterial::Settings> m_EditTracker;

		Entity m_Sphere1;
		Entity m_Sphere2;

		Entity m_Plane1;
		Entity m_Plane2;
	};
}
