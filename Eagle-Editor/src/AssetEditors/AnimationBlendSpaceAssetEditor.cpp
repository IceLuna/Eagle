#include "egpch.h"
#include "AnimationBlendSpaceAssetEditor.h"
#include "../EditorResources.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Input/Input.h"
#include "Eagle/Components/Components.h"

#include <imgui.h>
#include <imgui/imgui_internal.h>
#include <implot.h>

namespace Eagle
{
	static const std::string s_XVarName = "X";
	static const std::string s_YVarName = "Y";

	AnimationBlendSpaceAssetEditor::AnimationBlendSpaceAssetEditor(const Ref<AssetAnimationBlendSpace>& asset)
		: AssetEditor(true, true)
		, m_Asset(asset)
	{
		m_DetailsWindowName = m_Asset->GetPath().u8string();
		m_PlotWindowName = m_Asset->GetPath().u8string() + "_Plot";

		m_Horizontal = m_Asset->GetHorizontalAxis();
		m_Vertical = m_Asset->GetVerticalAxis();
		m_PointsData = m_Asset->GetPointsData();

		CreateAnimGraphForViewport();

		// Setup scene
		{
			const auto& skAsset = m_Asset->GetSkeletalMesh();
			const auto& scene = GetCurrentScene();
			m_Entity = scene->CreateEntity();
			auto& comp = m_Entity.AddComponent<SkeletalMeshComponent>();
			comp.AnimType = SkeletalMeshComponent::AnimationType::Graph;
			comp.SetMeshAsset(skAsset);
			comp.SetAnimationGraphAsset(m_AnimGraph);

			auto& camera = scene->GetEditorCamera();
			camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
			camera.LookAt(glm::vec3(0, 0, 0));
			const glm::vec3 cameraDir = camera.GetForwardVector();

			const auto& aabb = skAsset->GetMesh()->GetAABB();
			const glm::vec3 center = aabb.Center();
			camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.f); // Move back
			camera.LookAt(center);
		}
	}

	void AnimationBlendSpaceAssetEditor::OnImGuiRender(bool* pOpen)
	{
		bAxisLimitsChanged = false;
		bool bChanged = false;

		bChanged |= DrawDetails(pOpen);
		bChanged |= DrawPlot();
		DrawViewport(true, m_DetailsWindowName);

		if (bChanged)
		{
			m_Asset->SetPointsData(m_PointsData);

			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}
	}

	bool AnimationBlendSpaceAssetEditor::DrawDetails(bool* pOpen)
	{
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_DetailsWindowName.c_str(), pOpen);

		UI::BeginPropertyGrid("AnimationBlendSpaceDetails");
		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Animation Blend Space");

		ImGui::Separator();

		BlendSpaceEventsTriggerMode mode = m_Asset->GetEventsTriggerMode();
		if (UI::ComboEnum("Events Trigger Mode", mode))
		{
			m_Asset->SetEventsTriggerMode(mode);
			bChanged = true;
		}

		UI::EndPropertyGrid();
		ImGui::Separator();

		DrawVisualizationData();

		if (DrawAxisTreeNode("Horizontal Axis", m_Horizontal))
		{
			m_Asset->SetHorizontalAxis(m_Horizontal);
			bChanged = true;
		}
		if (DrawAxisTreeNode("Vertical Axis", m_Vertical))
		{
			m_Asset->SetVerticalAxis(m_Vertical);
			bChanged = true;
		}

		bChanged |= DrawAddPointTreeNode();
		bChanged |= DrawAllPointsTreeNode();

		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		return bChanged;
	}

	bool AnimationBlendSpaceAssetEditor::DrawPlot()
	{
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_PlotWindowName.c_str(), nullptr);

		ImGui::Checkbox("Draw Triangulation", &bDrawTriangulation);
		if (m_SelectedPointIdx != s_InvalidIndex)
		{
			auto& pointData = m_PointsData[m_SelectedPointIdx];
			ImGui::SameLine();
			const float availWidth = ImGui::GetContentRegionAvail().x;
			ImGui::PushItemWidth(availWidth * 0.25f);
			bChanged |= ImGui::InputDouble(m_Horizontal.Name.c_str(), &pointData.Vertex.Coord.x);
			ImGui::SameLine();
			bChanged |= ImGui::InputDouble(m_Vertical.Name.c_str(), &pointData.Vertex.Coord.y);
			ImGui::PopItemWidth();

			pointData.Vertex.Coord.x = glm::clamp(pointData.Vertex.Coord.x, m_Horizontal.Min, m_Horizontal.Max);
			pointData.Vertex.Coord.y = glm::clamp(pointData.Vertex.Coord.y, m_Vertical.Min, m_Vertical.Max);
		}

		const bool bVisualization = Input::IsKeyPressed(Key::LeftControl);
		const ImPlotFlags plotDefaultFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus;
		const ImPlotFlags plotFlags = plotDefaultFlags | (bVisualization ? ImPlotFlags_Crosshairs : ImPlotFlags_NoMouseText);

		const ImVec2 plotStartPos = ImGui::GetCursorScreenPos();
		const ImVec2 plotEndPos = plotStartPos + ImGui::GetContentRegionAvail();
		const ImVec2 mousePos = ImGui::GetMousePos();
		if (ImPlot::BeginPlot(m_PlotWindowName.c_str(), ImGui::GetContentRegionAvail(), plotFlags))
		{
			ImPlot::SetupAxes(m_Horizontal.Name.c_str(), m_Vertical.Name.c_str());
			ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, m_Horizontal.Min, m_Horizontal.Max);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, m_Vertical.Min, m_Vertical.Max);
			ImPlot::SetupAxesLimits(m_Horizontal.Min, m_Horizontal.Max, m_Vertical.Min, m_Vertical.Max, ImPlotCond_Once); // Fit whole graph once on init
			if (bAxisLimitsChanged)
			{
				ImPlot::SetupAxesLimits(m_Horizontal.Min, m_Horizontal.Max, m_Vertical.Min, m_Vertical.Max, ImPlotCond_Always);
			}

			const ImVec4 defaultColor = ImVec4(0.85f, 0.85f, 0.85f, 1);
			const ImVec4 selectedColor = ImVec4(0.45f, 0.45f, 0.75f, 1);

			size_t pointIdxToDelete = s_InvalidIndex;
			bool bAnyClicked = false;

			for (size_t i = 0; i < m_PointsData.size(); ++i)
			{
				ImGui::PushID(int(i));

				auto& pointData = m_PointsData[i];

				bool bClicked = false;
				bool bHovered = false;
				bool bHeld = false;

				bChanged |= ImPlot::DragPoint(int(i), &pointData.Vertex.Coord.x, &pointData.Vertex.Coord.y, m_SelectedPointIdx == i ? selectedColor : defaultColor, 4, ImPlotDragToolFlags_Clamp, &bClicked, &bHovered, &bHeld);
				if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					bClicked = true;
					ImGui::OpenPopup("AnimPoint Context Menu");
				}
				if (bClicked || (!bClicked && bHeld))
				{
					m_SelectedPointIdx = i;
				}
				bAnyClicked |= bClicked;

				if (ImGui::BeginPopup("AnimPoint Context Menu"))
				{
					if (ImGui::MenuItem("Delete"))
					{
						pointIdxToDelete = i;
					}
					ImGui::EndPopup();
				}

				// Clamp again after dragging to enforce limits
				pointData.Vertex.Coord.x = ImClamp(pointData.Vertex.Coord.x, double(m_Horizontal.Min), double(m_Horizontal.Max));
				pointData.Vertex.Coord.y = ImClamp(pointData.Vertex.Coord.y, double(m_Vertical.Min), double(m_Vertical.Max));

				ImGui::PopID();
			}

			const bool bMouseWithinPlot = mousePos.x >= plotStartPos.x && mousePos.x <= plotEndPos.x
				&& mousePos.y >= plotStartPos.y && mousePos.y <= plotEndPos.y;
			if (bMouseWithinPlot && !bAnyClicked && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				m_SelectedPointIdx = s_InvalidIndex;
			}

			if (m_SelectedPointIdx != s_InvalidIndex && Input::IsKeyPressed(Key::Delete))
			{
				pointIdxToDelete = m_SelectedPointIdx;
			}

			if (pointIdxToDelete != s_InvalidIndex)
			{
				RemovePoint(pointIdxToDelete);
				bChanged = true;
			}

			if (bDrawTriangulation)
			{
				const auto& triangulation = m_Asset->GetTriangulation();
				// We shouldn't use `m_PointsData` because selection will be flickering while a point is being dragged around, since we didn't yet regenerate the triangulation
				const auto& pointsData = m_Asset->GetPointsData();
				for (const auto& tri : triangulation)
				{
					const bool bSelected = m_SelectedPointIdx == s_InvalidIndex ? false
						: tri.V[0] == pointsData[m_SelectedPointIdx].Vertex || tri.V[1] == pointsData[m_SelectedPointIdx].Vertex || tri.V[2] == pointsData[m_SelectedPointIdx].Vertex;
					const float lineWidth = bSelected ? 3.f : 0.5f;
					glm::dvec2 points[2];

					points[0] = tri.V[0].Coord;
					points[1] = tri.V[1].Coord;
					ImPlot::SetNextLineStyle(ImVec4(0.45f, 0.45f, 0.25f, 1.f), lineWidth);
					ImPlot::PlotLine("##", &points[0].x, &points[0].y, 2, 0, 0, sizeof(glm::dvec2));

					points[1] = tri.V[2].Coord;
					ImPlot::SetNextLineStyle(ImVec4(0.45f, 0.45f, 0.25f, 1.f), lineWidth);
					ImPlot::PlotLine("##", &points[0].x, &points[0].y, 2, 0, 0, sizeof(glm::dvec2));

					points[0] = tri.V[1].Coord;
					ImPlot::SetNextLineStyle(ImVec4(0.45f, 0.45f, 0.25f, 1.f), lineWidth);
					ImPlot::PlotLine("##", &points[0].x, &points[0].y, 2, 0, 0, sizeof(glm::dvec2));
				}
			}

			if (bVisualization)
			{
				ImPlotPoint coords = ImPlot::GetPlotMousePos();
				auto& graph = m_Entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
				Cast<GraphVariableFloat>(graph->GetVariable(s_XVarName))->Value = float(coords.x);
				Cast<GraphVariableFloat>(graph->GetVariable(s_YVarName))->Value = float(coords.y);
			}

			ImPlot::EndPlot();
		}

		ImGui::End();

		return bChanged;
	}

	bool AnimationBlendSpaceAssetEditor::DrawAxisTreeNode(const char* name, BlendSpaceAxisSettings& axis)
	{
		constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		bool bChanged = false;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		bool treeOpened = ImGui::TreeNodeEx(name, flags);
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid(name);

			bChanged |= UI::PropertyText("Name", axis.Name);
			bAxisLimitsChanged |= UI::InputDouble("Min", axis.Min);
			bAxisLimitsChanged |= UI::InputDouble("Max", axis.Max);
			bChanged |= bAxisLimitsChanged;

			UI::EndPropertyGrid();
			ImGui::TreePop();
		}

		return bChanged;
	}

	bool AnimationBlendSpaceAssetEditor::DrawAddPointTreeNode()
	{
		constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		bool bChanged = false;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		bool treeOpened = ImGui::TreeNodeEx("Add Point", flags);
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid("Add Point");

			EditorResources::DrawAssetSelection("Animation", m_CurrentlyAddingPoint.Animation);
			UI::InputDouble(m_Horizontal.Name.c_str(), m_CurrentlyAddingPoint.Vertex.Coord.x);
			UI::InputDouble(m_Vertical.Name.c_str(), m_CurrentlyAddingPoint.Vertex.Coord.y);
			UI::InputFloat("Anim Speed", m_CurrentlyAddingPoint.AnimSpeed);

			m_CurrentlyAddingPoint.Vertex.Coord.x = glm::clamp(m_CurrentlyAddingPoint.Vertex.Coord.x, m_Horizontal.Min, m_Horizontal.Max);
			m_CurrentlyAddingPoint.Vertex.Coord.y = glm::clamp(m_CurrentlyAddingPoint.Vertex.Coord.y, m_Vertical.Min, m_Vertical.Max);

			UI::EndPropertyGrid();
			ImGui::Separator();

			if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				m_PointsData.push_back(m_CurrentlyAddingPoint);
				bChanged = true;
			}

			ImGui::TreePop();
		}

		return bChanged;
	}

	bool AnimationBlendSpaceAssetEditor::DrawAllPointsTreeNode()
	{
		constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		bool bChanged = false;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		bool treeOpened = ImGui::TreeNodeEx("All Points", flags);
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			if (!m_PointsData.empty())
			{
				UI::BeginPropertyGrid("All Points");
				const char* pointsLabel = m_PointsData.size() == 1 ? " Point" : " Points";
				if (UI::Button(std::to_string(m_PointsData.size()) + pointsLabel, "Remove all"))
				{
					bChanged = true;
					m_PointsData.clear();
					m_SelectedPointIdx = s_InvalidIndex;
				}
				UI::EndPropertyGrid();
				ImGui::Separator();
			}

			size_t pointIdxToDelete = s_InvalidIndex;
			
			for (size_t i = 0; i < m_PointsData.size(); ++i)
			{
				ImGui::PushID(int(i));

				const ImGuiTreeNodeFlags flags = (m_SelectedPointIdx == i ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow
					| ImGuiTreeNodeFlags_SpanAvailWidth;

				auto& pointData = m_PointsData[i];

				const std::string name = pointData.Animation ?
					pointData.Animation->GetPath().stem().u8string() + " (" + std::to_string(i) + ')'
					: '(' + std::to_string(i) + ')';

				const bool bOpened = ImGui::TreeNodeEx((void*)&m_PointsData[i], flags, name.c_str());

				if (ImGui::IsItemClicked())
				{
					m_SelectedPointIdx = i;
				}

				if (ImGui::BeginPopupContextItem())
				{
					m_SelectedPointIdx = i;
					if (ImGui::MenuItem("Delete"))
					{
						pointIdxToDelete = i;
					}
					ImGui::EndPopup();
				}

				if (bOpened)
				{
					UI::BeginPropertyGrid("All Points");

					bChanged |= EditorResources::DrawAssetSelection("Animation", pointData.Animation);
					bChanged |= UI::InputDouble(m_Horizontal.Name, pointData.Vertex.Coord.x);
					bChanged |= UI::InputDouble(m_Vertical.Name, pointData.Vertex.Coord.y);
					bChanged |= UI::InputFloat("Anim Speed", pointData.AnimSpeed);

					UI::EndPropertyGrid();

					pointData.Vertex.Coord.x = glm::clamp(pointData.Vertex.Coord.x, m_Horizontal.Min, m_Horizontal.Max);
					pointData.Vertex.Coord.y = glm::clamp(pointData.Vertex.Coord.y, m_Vertical.Min, m_Vertical.Max);
					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			ImGui::TreePop();

			if (pointIdxToDelete != s_InvalidIndex)
			{
				RemovePoint(pointIdxToDelete);
				bChanged = true;
			}
		}

		return bChanged;
	}

	void AnimationBlendSpaceAssetEditor::DrawVisualizationData()
	{
		constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		bool treeOpened = ImGui::TreeNodeEx("Visualization", flags);
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid("Visualization");

			auto& graph = m_Entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();

			auto xVar = Cast<GraphVariableFloat>(graph->GetVariable(s_XVarName));
			auto yVar = Cast<GraphVariableFloat>(graph->GetVariable(s_YVarName));

			UI::InputFloat(m_Horizontal.Name, xVar->Value, 0.f, 0.f, "You can also hold CTRL and move mouse on the plot");
			UI::InputFloat(m_Vertical.Name, yVar->Value, 0.f, 0.f, "You can also hold CTRL and move mouse on the plot");

			xVar->Value = (float)glm::clamp(double(xVar->Value), m_Horizontal.Min, m_Horizontal.Max);
			yVar->Value = (float)glm::clamp(double(yVar->Value), m_Vertical.Min, m_Vertical.Max);

			UI::EndPropertyGrid();
			ImGui::TreePop();
		}
	}

	void AnimationBlendSpaceAssetEditor::RemovePoint(size_t idx)
	{
		if (idx == s_InvalidIndex)
			return;

		if (m_SelectedPointIdx != s_InvalidIndex)
		{
			if (m_SelectedPointIdx == idx)
			{
				m_SelectedPointIdx = s_InvalidIndex;
			}
			else if (m_SelectedPointIdx > idx)
			{
				m_SelectedPointIdx--;
			}
		}
		m_PointsData.erase(m_PointsData.begin() + idx);
	}

	void AnimationBlendSpaceAssetEditor::CreateAnimGraphForViewport()
	{
		class LocalAssetAnimationGraph : public AssetAnimationGraph
		{
		public:
			LocalAssetAnimationGraph(const Ref<AssetSkeletalMesh>& sk, const GraphEditorSerializationData& data)
				: AssetAnimationGraph("", GUID{}, MakeRef<AnimationGraph>(sk), data) {
			}
		};

		GraphEditorSerializationData data{};

		// Create two vars in the graph
		{
			data.Variables.reserve(2);
			auto& xVar = data.Variables.emplace_back();
			xVar.Name = s_XVarName;
			xVar.Value = MakeRef<GraphVariableFloat>(float(m_Horizontal.Min));
			auto& yVar = data.Variables.emplace_back();
			yVar.Name = s_YVarName;
			yVar.Value = MakeRef<GraphVariableFloat>(float(m_Vertical.Min));
		}

		constexpr uint32_t outputNodeID = 1;
		constexpr uint32_t blendSpaceNodeID = 2;
		uint32_t nodeID = blendSpaceNodeID + 1; // Rest node IDs

		auto& nodes = data.Graph.Nodes;
		nodes.reserve(4); // Output, blend space, and two vars
		// Output node
		{
			auto& outputNode = nodes.emplace_back();
			outputNode.OwnerID = data.Graph.ID;
			outputNode.Name = "Output Pose";
			outputNode.NodeID = outputNodeID;
			outputNode.CachedOwnerID = GUID(0, 0);
			outputNode.CachedNodeID = 0;
			outputNode.Type = GraphNodeType::Node;

			auto& inputPin = outputNode.InputPins.emplace_back();
			inputPin.PinID = nodeID++;
		}

		// BlendSpace node
		{
			auto& bsNode = nodes.emplace_back();
			bsNode.OwnerID = data.Graph.ID;
			bsNode.Name = m_Asset->GetPath().stem().u8string();
			bsNode.NodeID = blendSpaceNodeID;
			bsNode.CachedOwnerID = GUID(0, 0);
			bsNode.CachedNodeID = 0;
			bsNode.Type = GraphNodeType::BlendSpace;
			bsNode.BlendSpace = m_Asset;

			bsNode.InputPins.resize(2);
			for (auto& input : bsNode.InputPins)
				input.PinID = nodeID++;

			bsNode.OutputPins.push_back(nodeID++);

			// Connect to output
			auto& connection = bsNode.OutputConnections.emplace_back();
			connection.NodeID = outputNodeID;
			connection.PinIndex = 0;
		}

		// X var
		{
			auto& varNode = nodes.emplace_back();
			varNode.OwnerID = data.Graph.ID;
			varNode.Name = s_XVarName;
			varNode.NodeID = nodeID++;
			varNode.CachedOwnerID = GUID(0, 0);
			varNode.CachedNodeID = 0;
			varNode.Type = GraphNodeType::Variable;

			varNode.OutputPins.push_back(nodeID++);

			// Connect to output
			auto& connection = varNode.OutputConnections.emplace_back();
			connection.NodeID = blendSpaceNodeID;
			connection.PinIndex = 0;
		}

		// Y var
		{
			auto& varNode = nodes.emplace_back();
			varNode.OwnerID = data.Graph.ID;
			varNode.Name = s_YVarName;
			varNode.NodeID = nodeID++;
			varNode.CachedOwnerID = GUID(0, 0);
			varNode.CachedNodeID = 0;
			varNode.Type = GraphNodeType::Variable;

			varNode.OutputPins.push_back(nodeID++);

			// Connect to output
			auto& connection = varNode.OutputConnections.emplace_back();
			connection.NodeID = blendSpaceNodeID;
			connection.PinIndex = 1;
		}

		m_AnimGraph = MakeRef<LocalAssetAnimationGraph>(m_Asset->GetSkeletalMesh(), data);
		m_AnimGraph->Compile();
	}

	void AnimationBlendSpaceAssetEditor::HandleFirstWindowRender(std::string_view windowName, std::string_view parentName)
	{
		const bool bFirstUseEver = (ImGui::GetCurrentWindow()->SetWindowDockAllowFlags & ImGuiCond_FirstUseEver) == ImGuiCond_FirstUseEver;

		if (bFirstUseEver && !parentName.empty())
		{
			ImGuiID parent_node = ImGui::DockBuilderAddNode();
			ImGui::DockBuilderSetNodePos(parent_node, ImGui::GetWindowPos());
			ImGui::DockBuilderSetNodeSize(parent_node, ImGui::GetWindowSize());
			ImGuiID nodeDetails; // Main window
			ImGuiID nodeViewport;
			ImGui::DockBuilderSplitNode(parent_node, ImGuiDir_Left, 0.5f, &nodeViewport, &nodeDetails);

			ImGui::DockBuilderDockWindow(parentName.data(), nodeDetails);
			ImGui::DockBuilderDockWindow(windowName.data(), nodeViewport);

			ImGuiID nodePlot;
			ImGui::DockBuilderSplitNode(nodeViewport, ImGuiDir_Up, 0.5f, &nodeViewport, &nodePlot);
			ImGui::DockBuilderDockWindow(m_PlotWindowName.c_str(), nodePlot);
			ImGui::DockBuilderDockWindow(windowName.data(), nodeViewport);

			// Disable tab bar for plot & viewport
			if (ImGuiDockNode* dock = ImGui::DockContextFindNodeByID(GImGui, nodePlot))
			{
				dock->SetLocalFlags(ImGuiDockNodeFlags_NoTabBar);
			}
			if (ImGuiDockNode* dock = ImGui::DockContextFindNodeByID(GImGui, nodeViewport))
			{
				dock->SetLocalFlags(ImGuiDockNodeFlags_NoTabBar);
			}

			ImGui::SetWindowSize(ImVec2(720.f * 2.f, 560.f));
		}
	}
}