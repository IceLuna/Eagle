#pragma once

#include "AssetEditor.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Utils/DelaunayTriangulation.h"

#include <glm/glm.hpp>

namespace Eagle
{
	class AssetAnimationBlendSpace;
	class AssetAnimationGraph;

	class AnimationBlendSpaceAssetEditor : public AssetEditor
	{
	public:
		AnimationBlendSpaceAssetEditor(const Ref<AssetAnimationBlendSpace>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }
	
	protected:
		void HandleFirstWindowRender(std::string_view windowName, std::string_view parentName) override;

	private:
		bool DrawDetails(bool* pOpen);
		bool DrawPlot();
		bool DrawAxisTreeNode(const char* name, BlendSpaceAxisSettings& axis);
		bool DrawAddPointTreeNode();
		bool DrawAllPointsTreeNode();
		void DrawVisualizationData();

		void RemovePoint(size_t idx);

		void CreateAnimGraphForViewport();

	private:
		Ref<AssetAnimationBlendSpace> m_Asset;
		Ref<AssetAnimationGraph> m_AnimGraph;
		Entity m_Entity;

		std::string m_DetailsWindowName;
		std::string m_PlotWindowName;

		BlendSpaceVertex m_CurrentlyAddingPoint;
		std::vector<BlendSpaceVertex> m_PointsData;
		constexpr static size_t s_InvalidIndex = size_t(-1);
		size_t m_SelectedPointIdx = s_InvalidIndex;

		BlendSpaceAxisSettings m_Horizontal;
		BlendSpaceAxisSettings m_Vertical;
		bool bAxisLimitsChanged = false;
		bool bDrawTriangulation = true;
		bool m_bPlotHovered = false;
	};
}
