#include "egpch.h"
#include "SkeletalMeshAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/Project.h"

namespace Eagle
{
	static const char* s_PositionSolverIterationsHelpMsg = "The solver iteration count determines how accurately joints and contacts are resolved. "
		"If you are having trouble with jointed bodies oscillating and behaving erratically, then "
		"setting a higher position iteration count may improve their stability.";
	static const char* s_VelocitySolverIterationsHelpMsg = "If intersecting bodies are being depenetrated too violently, increase the number of velocity "
		"iterations.More velocity iterations will drive the relative exit velocity of the intersecting "
		"objects closer to the correct value given the restitution.";
	static const char* s_CollisionDetectionTypeHelpMsg = "When continuous collision detection (or CCD) is turned on, the affected rigid bodies will not go through other objects at high velocities (a problem also known as tunnelling). "
		"A cheaper but less robust approach is called speculative CCD";

	static bool HasBoneWithName(const BoneNode& node, const std::string& name)
	{
		if (node.Name == name)
			return true;

		for (const auto& child : node.Children)
			if (HasBoneWithName(child, name))
				return true;

		return false;
	}

	static std::string GetUniqueBoneName(const SkeletalMeshInfo& skeletalInfo)
	{
		const std::string defaultName = "Virtual Bone";
		uint32_t i = 0;

		std::string name = defaultName;
		while (HasBoneWithName(skeletalInfo.RootBone, name))
			name = defaultName + '_' + std::to_string(i++);

		return name;
	}

	// Returns true if found
	static bool GetRootBone_Internal(BoneNode& node, const BonesMap& bonesMap, BoneNode** outNode = nullptr)
	{
		if (auto it = bonesMap.find(node.Name); it != bonesMap.end())
		{
			*outNode = &node;
			return true;
		}

		for (auto& child : node.Children)
		{
			if (GetRootBone_Internal(child, bonesMap, outNode))
			{
				return true;
			}
		}

		return false;
	}

	static BoneNode& GetRootBone(BoneNode& node, const BonesMap& bonesMap)
	{
		BoneNode* rootNode = nullptr;
		if (GetRootBone_Internal(node, bonesMap, &rootNode))
		{
			return *rootNode;
		}
		return node;
	}

	static bool DrawMeshSelection(Ref<AssetBaseMesh>& modifyingAsset)
	{
		const ImVec2 previewSize = ImVec2(32.f, 32.f);
		Ref<Eagle::Image> preview = EditorResources::GetAssetPreview(modifyingAsset);
		bool bResult = false;

		if (preview)
		{
			const ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::SetCursorScreenPos(p);

			UI::Image(preview, previewSize);

			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		}

		const std::string assetName = modifyingAsset ? modifyingAsset->GetPath().stem().u8string() : "None";
		bool bBeginCombo = ImGui::BeginCombo("##", assetName.c_str(), 0);

		if (bBeginCombo)
		{
			const int noneOffset = 1; // It's required to correctly set what item is selected, since the first one is alwasy `None`, we need to offset it
			const int nonePosition = 0;
			int currentItemIdx = nonePosition;
			// Initially find currently selected asset to scroll to it.
			if (modifyingAsset)
			{
				uint32_t i = noneOffset;
				const auto& allAssets = AssetManager::GetAssets();
				for (const auto& [unused, asset] : allAssets)
				{
					if (asset == modifyingAsset)
					{
						currentItemIdx = i;
						break;
					}
					if (const auto& castedAsset = Cast<AssetBaseMesh>(asset))
						i++;
				}
			}
		
			// Draw none
			{
				const bool bSelected = (currentItemIdx == nonePosition);
				if (ImGui::Selectable("None", bSelected))
					currentItemIdx = nonePosition;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = nonePosition;
					modifyingAsset.reset();
					bResult = true;
				}
			}
		
			//Drawing all existing asset
			const auto& allAssets = AssetManager::GetAssets();
			uint32_t i = noneOffset;
			for (const auto& [path, asset] : allAssets)
			{
				const auto castedAsset = Cast<AssetBaseMesh>(asset);
				if (!castedAsset)
					continue;

				const bool bSelected = currentItemIdx == i;
				ImGui::PushID((void*)asset->GetGUID().GetHash());

				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowOverlap, { 0.0f, previewSize.y });
				bSelectableTriggered |= ImGui::IsItemClicked();

				{
					ImGui::SameLine();
					Ref<Eagle::Image> preview = ThumbnailCache::Get(asset);
					if (!preview)
					{
						if (ThumbnailCache::IsRenderableAssetType(asset->GetAssetType()))
						{
							if (ThumbnailCache::Render(asset))
							{
								preview = ThumbnailCache::Get(asset);
							}
						}
					}
					UI::Image(preview ? preview : Texture2D::NoneIconTexture->GetImage(), previewSize);
				}

				ImGui::SameLine();
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + previewSize.y * 0.25f);
				ImGui::Text("%s", path.stem().u8string().c_str());

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableTriggered)
				{
					currentItemIdx = i;

					modifyingAsset = castedAsset;
					bResult = true;
				}
				++i;
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		return bResult;
	}

	static Entity SpawnMeshVisualization(const Ref<AssetBaseMesh>& mesh, const std::string& name, const Ref<Scene>& scene)
	{
		Entity e = scene->CreateEntity(name);
		if (mesh->GetAssetType() == AssetType::StaticMesh)
		{
			auto& comp = e.AddComponent<StaticMeshComponent>();
			comp.SetMeshAsset(Cast<AssetStaticMesh>(mesh));
		}
		else if (mesh->GetAssetType() == AssetType::SkeletalMesh)
		{
			auto& comp = e.AddComponent<SkeletalMeshComponent>();
			comp.SetMeshAsset(Cast<AssetSkeletalMesh>(mesh));
		}
		else
		{
			EG_CORE_ASSERT(false);
		}

		return e;
	}

	// Returns true if component is valid
	template <typename ColliderType, typename It>
	static bool DrawColliderCheckbox(const char* label, const std::string& boneName, const Ref<Scene>& scene, It& it, std::unordered_map<std::string, AttachedColliderData>& map)
	{
		const bool bValid = it != map.end();
		bool bHasCollider = bValid && it->second.Entity.HasComponent<ColliderType>();

		if (ImGui::Checkbox(label, &bHasCollider))
		{
			if (!bValid)
			{
				Entity e = scene->CreateEntity();
				it = map.insert(std::make_pair(boneName, AttachedColliderData{e})).first;
			}

			if (bHasCollider)
			{
				auto& comp = it->second.Entity.AddComponent<ColliderType>();
				comp.SetShowCollision(true);
				comp.SetCollisionGroup(s_CollisionGroupNone);
				comp.SetInteractingCollisionGroup(s_CollisionGroupNone);
			}
			else
			{
				it->second.Entity.RemoveComponent<ColliderType>();
			}
		}

		if (bValid)
		{
			if constexpr (std::is_same_v<ColliderType, BoxColliderComponent>)
			{
				it->second.bHasBox = bHasCollider;
			}
			else if constexpr (std::is_same_v<ColliderType, SphereColliderComponent>)
			{
				it->second.bHasSphere = bHasCollider;
			}
			else if constexpr (std::is_same_v<ColliderType, CapsuleColliderComponent>)
			{
				it->second.bHasCapsule = bHasCollider;
			}
			else
			{
				static_assert(false); // Unknown type
			}
		}

		return bHasCollider;
	}

	// Returns true if it changed
	bool SkeletalMeshAssetEditor::DrawSkeletalTree(const SkeletalMeshInfo& skeletalInfo, BoneNode& node, size_t baseHash, bool* outDelete, const glm::mat4& baseTransform, const std::string& parentName)
	{
		size_t hash = std::hash<std::string>()(node.Name);
		HashCombine(hash, baseHash);
		bool bChanged = false;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
			| (node.Children.size() ? 0 : ImGuiTreeNodeFlags_Leaf) | (m_SelectedBone && m_SelectedBone->Name == node.Name ? ImGuiTreeNodeFlags_Selected : 0);

		bool opened = ImGui::TreeNodeEx((void*)hash, flags, node.Name.c_str());
		const glm::mat4 worldTr = baseTransform * node.Transformation;

		if (ImGui::IsItemClicked())
		{
			m_SelectedBoneName = node.Name;
			m_SelectedBoneParentName = parentName;
			m_SelectedBone = &node;
		}

		if (ImGui::BeginPopupContextItem(nullptr))
		{
			if (ImGui::MenuItem("Add virtual bone"))
			{
				auto& bone = node.Children.emplace_back();
				bone.Name = GetUniqueBoneName(skeletalInfo);
				bone.Transformation = glm::mat4(1.f);
				bone.bVirtualBone = true;
				bChanged = true;
			}

			if (ImGui::BeginMenu("Attach Mesh (visualization only)"))
			{
				auto it = m_AttachedToBonesMeshes.find(node.Name);
				Ref<AssetBaseMesh> mesh = it != m_AttachedToBonesMeshes.end() ? it->second.Mesh : nullptr;

				if (DrawMeshSelection(mesh))
				{
					auto& scene = GetCurrentScene();
					if (it != m_AttachedToBonesMeshes.end())
					{
						scene->DestroyEntity(it->second.Entity);
						m_AttachedToBonesMeshes.erase(it);
					}

					if (mesh)
					{
						Entity e = SpawnMeshVisualization(mesh, node.Name, scene);
						m_AttachedToBonesMeshes[node.Name] = { mesh, e };
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Attach primitive (visualization only)"))
			{
				auto& scene = GetCurrentScene();
				auto it = m_AttachedToBonesColliders.find(node.Name);
				const bool bValid = it != m_AttachedToBonesColliders.end();
				const bool bHasBox = DrawColliderCheckbox<BoxColliderComponent>("Box", node.Name, scene, it, m_AttachedToBonesColliders);
				const bool bHasSphere = DrawColliderCheckbox<SphereColliderComponent>("Sphere", node.Name, scene, it, m_AttachedToBonesColliders);
				const bool bHasCapsule = DrawColliderCheckbox<CapsuleColliderComponent>("Capsule", node.Name, scene, it, m_AttachedToBonesColliders);

				if (bValid && !bHasBox && !bHasSphere && !bHasCapsule)
				{
					// No colliders, so delete the entity
					scene->DestroyEntity(it->second.Entity);
					m_AttachedToBonesColliders.erase(it);
				}
				ImGui::EndMenu();
			}

			if (node.bVirtualBone)
			{
				ImGui::Separator();
				if (ImGui::MenuItem("Delete"))
				{
					if (m_SelectedBone && m_SelectedBone->Name == node.Name)
					{
						m_SelectedBoneName.clear();
						m_SelectedBone = nullptr;
					}

					*outDelete = true;
					bChanged = true;
				}
			}

			ImGui::EndPopup();
		}

		if (opened)
		{
			for (auto it = node.Children.begin(); it != node.Children.end(); )
			{
				auto& child = *it;

				bool bDelete = false;
				bChanged |= DrawSkeletalTree(skeletalInfo, child, baseHash, &bDelete, worldTr, node.Name);

				if (bDelete)
				{
					OnBoneNodeDeletion(child);
					it = node.Children.erase(it);
				}
				else
					++it;
			}

			ImGui::TreePop();
		}

		return bChanged;
	}

	bool SkeletalMeshAssetEditor::DrawRagdollTree(SkeletalRagdollBone& node, size_t baseHash)
	{
		size_t hash = std::hash<std::string>()(node.Name);
		HashCombine(hash, baseHash);
		bool bChanged = false;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
			| (node.Children.size() ? 0 : ImGuiTreeNodeFlags_Leaf) | (m_SelectedRagdollBone && m_SelectedRagdollBone->Name == node.Name ? ImGuiTreeNodeFlags_Selected : 0);

		bool opened = ImGui::TreeNodeEx((void*)hash, flags, node.Name.c_str());

		if (ImGui::IsItemClicked())
		{
			m_SelectedRagdollBoneName = node.Name;
			m_SelectedRagdollBone = &node;
		}

		if (opened)
		{
			for (auto& child : node.Children)
			{
				bChanged |= DrawRagdollTree(child, baseHash);
			}

			ImGui::TreePop();
		}

		return bChanged;
	}

	SkeletalMeshAssetEditor::SkeletalMeshAssetEditor(const Ref<AssetSkeletalMesh>& asset)
		: AssetEditor(true, true), m_Asset(asset)
	{
		const auto& mesh = m_Asset->GetMesh();
		m_MinRagdollBoneSize = mesh->GetMinRagdollBoneSize();
		m_Twist = mesh->GetRagdollMaxTwist();
		m_Swing = mesh->GetRagdollMaxSwing();
		m_CollisionDetection = mesh->GetCollisionDetectionType();
		m_CollisionGroup = (uint32_t)mesh->GetCollisionGroup();
		m_InteractingCollisionGroup = (uint32_t)mesh->GetInteractingCollisionGroup();

		const auto& scene = GetCurrentScene();

		m_Entity = scene->CreateEntity("SkeletalMeshAssetEditor");
		auto& component = m_Entity.AddComponent<SkeletalMeshComponent>();
		component.SetMeshAsset(m_Asset);
		component.SetRagdollEnabled(true);

		auto& camera = scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = mesh->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.f); // Move back
		camera.LookAt(center);

		auto& sceneRenderer = scene->GetSceneRenderer();
		auto settings = sceneRenderer->GetOptions();
		settings.bEnableDebugLinesDepthTest = bEnableDebugLinesDepthTest;
		sceneRenderer->SetOptions(settings);
		OnSimulateRagdollChanged();
	}

	void SkeletalMeshAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const auto& scene = GetCurrentScene();
		auto& component = m_Entity.GetComponent<SkeletalMeshComponent>();
		auto& mesh = m_Asset->GetMesh();
		const size_t verticesCount = mesh->GetVerticesCount();
		size_t indicesCount = 0;
		for (uint32_t i = 0; i < mesh->GetMaterialSlotsCount(); ++i)
			indicesCount += mesh->GetIndicesCount(i);
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		const std::string windowName = m_Asset->GetPath().u8string();
		ImGui::Begin(windowName.c_str(), pOpen);
		UI::TextWithSeparator("Data");

		UI::BeginPropertyGrid("SkeletalMeshDetails");
		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Skeletal Mesh");
		UI::Text("Vertices", std::to_string(verticesCount));
		UI::Text("Indices", std::to_string(indicesCount));
		UI::Text("Vertices Mem Usage (Kb)", std::to_string(verticesCount * sizeof(SkeletalVertex) / 1024));
		UI::Text("Indices Mem Usage (Kb)", std::to_string(indicesCount * sizeof(Index) / 1024));

		UI::TextWithSeparator("Materials");

		const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
		for (uint32_t i = 0; i < materialsCount; ++i)
		{
			auto materialAsset = mesh->GetMaterialAsset(i);
			if (EditorResources::DrawAssetSelection("Material " + std::to_string(i), materialAsset))
			{
				mesh->SetMaterialAsset(i, materialAsset);
				component.SetMaterialAsset(i, materialAsset);
				bChanged = true;
			}
		}

		auto& skeletalComp = m_Entity.GetComponent<SkeletalMeshComponent>();
		UI::TextWithSeparator("Preview Settings");
		if (EditorResources::DrawAssetSelection("Animation", m_PreviewAnimation))
		{
			skeletalComp.SetAnimationAsset(m_PreviewAnimation);
			if (!m_PreviewAnimation)
			{
				m_Entity.SetWorldLocation({});
			}
		}
		UI::PropertyDrag("Animation Playback Speed", skeletalComp.ClipPlaybackSpeed, 0.1f);

		{
			const bool bHasAnim = m_PreviewAnimation.operator bool();
			float current = bHasAnim ? skeletalComp.CurrentClipPlayTime : 0.f;
			const float duration = bHasAnim ? m_PreviewAnimation->GetAnimation()->Duration : 1.f;
			if (!bHasAnim)
				UI::PushItemDisabled();

			if (UI::PropertySlider("Animation Position", current, 0.f, duration))
			{
				skeletalComp.CurrentClipPlayTime = glm::clamp(current, 0.f, m_PreviewAnimation->GetAnimation()->Duration);
				skeletalComp.PrevClipPlayTime = skeletalComp.CurrentClipPlayTime;
			}

			if (!bHasAnim)
				UI::PopItemDisabled();
		}

		if (UI::Property("Visualize ragdoll bones", bVisualizeRagdollBones))
		{
			skeletalComp.SetShowRagdollCollision(bVisualizeRagdollBones);
		}
		UI::Property("Visualize bones", scene->bDrawBones);
		UI::Property("Visualize bone direction", bVisualizeBoneDirection);
		if (UI::Property("Debug lines depth test", bEnableDebugLinesDepthTest, "If disabled, debug lines will be drawn over everything"))
		{
			auto& sceneRenderer = scene->GetSceneRenderer();
			auto settings = sceneRenderer->GetOptions();
			settings.bEnableDebugLinesDepthTest = bEnableDebugLinesDepthTest;
			sceneRenderer->SetOptions(settings);
		}
		UI::EndPropertyGrid();

		{
			const bool bDisableRotation = bSimulate && m_OpenedTab == OpenedTabType::Ragdoll;
			if (bDisableRotation)
				UI::PushItemDisabled();

			glm::quat quat = m_Entity.GetWorldRotation().GetQuat();
			if (UI::DrawQuatControl("Mesh Rotation(Quat)", quat, glm::quat{ 1, 0, 0, 0 }, 140.f))
			{
				m_Entity.SetWorldRotation(glm::quat(quat.w, quat.x, quat.y, quat.z));

				if (m_OpenedTab == OpenedTabType::Ragdoll)
				{
					skeletalComp.SetRagdollEnabled(false);
					skeletalComp.SetRagdollEnabled(true);
					skeletalComp.SetShowRagdollCollision(bVisualizeRagdollBones);
				}
			}

			if (bDisableRotation)
				UI::PopItemDisabled();
		}

		size_t assetHash = m_Asset->GetGUID().GetHash();

		ImGui::Separator();
		{
			if (ImGui::BeginTabBar("SkeletalMeshAssetEditorTabBar"))
			{
				bChanged |= DrawSkeletalTab(mesh, assetHash);
				bChanged |= DrawRagdollTab(mesh, assetHash);
				ImGui::EndTabBar();
			}
		}

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		if (bVisualizeBoneDirection)
		{
			if (m_OpenedTab == OpenedTabType::Skeletal && m_SelectedBone)
			{
				Transform transform = GetBoneWorldTransform(m_SelectedBoneName);
				scene->DrawArrow(transform.Location, transform.Location + Math::GetForwardVector(transform.Rotation) * 0.2f, Math::GetUpVector(transform.Rotation));
			}
			else if (m_OpenedTab == OpenedTabType::Ragdoll && m_SelectedRagdollBone)
			{
				Transform transform = GetSelectedRagdollBoneWorldTransform() + m_SelectedRagdollBone->Settings.UserOffset;
				scene->DrawArrow(transform.Location, transform.Location + Math::GetForwardVector(transform.Rotation) * 0.2f, Math::GetUpVector(transform.Rotation));
			}
		}

		// Set preview mesh transform to bone transform
		{
			if (bSimulate && m_OpenedTab == OpenedTabType::Ragdoll)
			{
				const auto& scene = GetCurrentScene();
				for (auto& [boneName, data] : m_AttachedToBonesMeshes)
				{
					Transform boneTr = GetBoneWorldTransform(boneName);
					Entity runtimeEntity = scene->GetEntityByGUID(data.Entity.GetGUID());
					runtimeEntity.SetWorldTransform(boneTr);
				}
				for (auto& [boneName, data] : m_AttachedToBonesColliders)
				{
					Transform boneTr = GetBoneWorldTransform(boneName);
					Entity runtimeEntity = scene->GetEntityByGUID(data.Entity.GetGUID());
					runtimeEntity.SetWorldTransform(boneTr);
				}
			}
			else
			{
				for (auto& [boneName, data] : m_AttachedToBonesMeshes)
				{
					Transform boneTr = GetBoneWorldTransform(boneName);
					data.Entity.SetWorldTransform(boneTr);
				}
				for (auto& [boneName, data] : m_AttachedToBonesColliders)
				{
					Transform boneTr = GetBoneWorldTransform(boneName);
					data.Entity.SetWorldTransform(boneTr);
				}
			}
		}

		const bool bUpdateAnimation = m_PreviewAnimation.operator bool();
		DrawViewport(bUpdateAnimation, windowName);
		bChanged |= bGuizmoChanged;
		bGuizmoChanged = false;

		if (bChanged)
		{
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}
	}

	bool SkeletalMeshAssetEditor::DrawSkeletalTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash)
	{
		const bool bTabOpened = ImGui::BeginTabItem("Skeletal tree");
		if (!bTabOpened)
			return false;

		if (m_OpenedTab != OpenedTabType::Skeletal)
		{
			m_Entity.GetComponent<SkeletalMeshComponent>().SetRagdollEnabled(false);
			SetSimulationEnabled(false);
			DeletePlane();
		}
		m_OpenedTab = OpenedTabType::Skeletal;

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		bool bChanged = false;
		const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash++), treeFlags, "Skeletal tree");
		if (bOpened)
		{
			auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
			auto& root = GetRootBone(skeletalInfo.RootBone, skeletalInfo.BoneInfoMap);
			bChanged |= DrawSkeletalTree(skeletalInfo, root, assetHash);

			if (m_SelectedBone)
			{
				ImGui::Separator();

				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

				bool bOpenedDetails = ImGui::TreeNodeEx((void*)(assetHash++), flags, "Details");
				if (bOpenedDetails)
				{
					if (m_SelectedBone->bVirtualBone == false)
						UI::PushItemDisabled();

					Transform boneTransform = Math::DecomposeTransformMatrix(m_SelectedBone->Transformation);

					bool bTransformChanged = false;

					bool bStoppedEditing = false;
					if (UI::InputText("Name", m_SelectedBoneName, ImGuiInputTextFlags_EnterReturnsTrue, "Only user-created (virtual) bones can be modified"))
						bStoppedEditing = true;

					// Lost focus, stop editing
					if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						bStoppedEditing = true;

					if (bStoppedEditing && m_SelectedBoneName != m_SelectedBone->Name)
					{
						if (HasBoneWithName(skeletalInfo.RootBone, m_SelectedBoneName))
						{
							Application::Get().GetImGuiLayer()->AddMessage("Failed to rename the bone. The name is already taken!");
							m_SelectedBoneName = m_SelectedBone->Name;
						}
						else
						{
							m_SelectedBone->Name = m_SelectedBoneName;
							bChanged = true;
						}
					}

					glm::quat quat = boneTransform.Rotation.GetQuat();

					bTransformChanged |= UI::DrawVec3Control("Location", boneTransform.Location, glm::vec3{ 0.f });
					if (UI::DrawQuatControl("Rotation (Quat)", quat))
					{
						boneTransform.Rotation = quat;
						bTransformChanged = true;
					}
					if (UI::DrawVec3Control("Scale", boneTransform.Scale3D, glm::vec3{ 1.f }))
					{
						constexpr float epsilon = 0.0001f;
						const glm::bvec3 bZero = glm::epsilonEqual(boneTransform.Scale3D, glm::vec3(0), epsilon);
						for (glm::length_t i = 0; i < bZero.length(); ++i)
						{
							if (bZero[i])
								boneTransform.Scale3D[i] = epsilon;
						}

						bTransformChanged = true;
					}

					if (bTransformChanged)
					{
						m_SelectedBone->Transformation = Math::ToTransformMatrix(boneTransform);
						bChanged = true;
					}

					if (m_SelectedBone->bVirtualBone == false)
						UI::PopItemDisabled();

					ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}
	
		ImGui::EndTabItem();
		return bChanged;
	}

	bool SkeletalMeshAssetEditor::DrawRagdollTab(const Ref<SkeletalMesh>& mesh, size_t& assetHash)
	{
		const bool bTabOpened = ImGui::BeginTabItem("Ragdoll tree");
		if (!bTabOpened)
			return false;

		if (m_OpenedTab != OpenedTabType::Ragdoll)
		{
			auto& comp = m_Entity.GetComponent<SkeletalMeshComponent>();
			comp.SetRagdollEnabled(true);
			comp.SetShowRagdollCollision(bVisualizeRagdollBones);
			SetSimulationEnabled(bSimulate);
			CreatePlane();
		}
		m_OpenedTab = OpenedTabType::Ragdoll;

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		bool bChanged = false;
		const bool bOpened = ImGui::TreeNodeEx((void*)(assetHash++), treeFlags, "Ragdoll tree");
		if (bOpened)
		{
			auto& mesh = m_Asset->GetMesh();
			DrawRagdollTree(mesh->GetRagdollRoot(), assetHash);

			if (m_SelectedRagdollBone)
			{
				ImGui::Separator();

				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

				bool bOpenedDetails = ImGui::TreeNodeEx((void*)(assetHash++), flags, "Details");
				if (bOpenedDetails)
				{
					bool bRagdollChanged = false;
					UI::BeginPropertyGrid("Details");
					UI::Text("Name", m_SelectedRagdollBoneName);
					if (UI::PropertyDrag("Position Solver Iterations", m_SelectedRagdollBone->Settings.PositionSolverIterations, 1, PhysicsSettings::MinPositionSolverIterations, PhysicsSettings::MaxPositionSolverIterations, s_PositionSolverIterationsHelpMsg))
					{
						m_SelectedRagdollBone->Settings.PositionSolverIterations = glm::clamp(m_SelectedRagdollBone->Settings.PositionSolverIterations, PhysicsSettings::MinPositionSolverIterations, PhysicsSettings::MaxPositionSolverIterations);
						bRagdollChanged = true;
					}
					if (UI::PropertyDrag("Velocity Solver Iterations", m_SelectedRagdollBone->Settings.VelocitySolverIterations, 1, PhysicsSettings::MinVelocitySolverIterations, PhysicsSettings::MaxVelocitySolverIterations, s_VelocitySolverIterationsHelpMsg))
					{
						m_SelectedRagdollBone->Settings.VelocitySolverIterations = glm::clamp(m_SelectedRagdollBone->Settings.VelocitySolverIterations, PhysicsSettings::MinVelocitySolverIterations, PhysicsSettings::MaxVelocitySolverIterations);
						bRagdollChanged = true;
					}
					if (UI::PropertyDrag("Mass", m_SelectedRagdollBone->Settings.Mass, 0.5f))
					{
						m_SelectedRagdollBone->Settings.Mass = glm::max(0.f, m_SelectedRagdollBone->Settings.Mass);
						bRagdollChanged = true;
					}
					if (UI::PropertyDrag("Linear Damping", m_SelectedRagdollBone->Settings.LinearDamping, 0.1f))
					{
						m_SelectedRagdollBone->Settings.LinearDamping = glm::max(0.f, m_SelectedRagdollBone->Settings.LinearDamping);
						bRagdollChanged = true;
					}
					if (UI::PropertyDrag("Angular Damping", m_SelectedRagdollBone->Settings.AngularDamping, 0.1f))
					{
						m_SelectedRagdollBone->Settings.AngularDamping = glm::max(0.f, m_SelectedRagdollBone->Settings.AngularDamping);
						bRagdollChanged = true;
					}
					bRagdollChanged |= EditorResources::DrawAssetSelection("Physics Material", m_SelectedRagdollBone->Settings.Material);
					bRagdollChanged |= UI::ComboEnum("Shape", m_SelectedRagdollBone->Settings.Shape);
					bRagdollChanged |= UI::Property("Enable Simulation", m_SelectedRagdollBone->Settings.bEnableSimulation);
					bRagdollChanged |= UI::Property("Enable Collision", m_SelectedRagdollBone->Settings.bEnableCollision);

					UI::EndPropertyGrid();
					ImGui::Separator();

					Transform& boneTransform = m_SelectedRagdollBone->Settings.UserOffset;

					bool bTransformChanged = false;
					glm::quat quat = boneTransform.Rotation.GetQuat();

					bTransformChanged |= UI::DrawVec3Control("Location", boneTransform.Location, glm::vec3{ 0.f });
					if (UI::DrawQuatControl("Rotation (Quat)", quat))
					{
						boneTransform.Rotation = quat;
						bTransformChanged = true;
					}
					if (UI::DrawVec3Control("Scale", boneTransform.Scale3D, glm::vec3{ 1.f }))
					{
						constexpr float epsilon = 0.00001f;
						const glm::bvec3 bZero = glm::epsilonEqual(boneTransform.Scale3D, glm::vec3(0), epsilon);
						for (glm::length_t i = 0; i < bZero.length(); ++i)
						{
							if (bZero[i])
								boneTransform.Scale3D[i] = epsilon;
						}
						bTransformChanged = true;
					}

					if (bTransformChanged)
					{
						bRagdollChanged = true;
					}

					bChanged |= bRagdollChanged;

					ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}

		ImGui::Separator();

		constexpr float thickness = 2.5f;
		const auto& collisionGroups = Project::GetAllCollisionGroups();
		bool bRegenerate = false; // TODO v0.7 Should always do it on change?

		UI::BeginPropertyGrid("Ragdoll props");
		if (UI::PropertyDrag("Min ragdoll bone size", m_MinRagdollBoneSize, 0.05f, 0.01f))
			m_MinRagdollBoneSize = glm::max(m_MinRagdollBoneSize, 0.05f);
		UI::PropertyDrag("Max Twist angle", m_Twist, 1.f, 0.01f, 180.f);
		UI::PropertyDrag("Max Swing angle", m_Swing, 1.f, 0.01f, 180.f);
		bRegenerate |= UI::ComboEnum<CollisionDetectionType>("Collision Detection", m_CollisionDetection, s_CollisionDetectionTypeHelpMsg);

		UI::TextWithSeparator("Collision Groups", thickness, "Collision groups it belongs to");
		bRegenerate |= UI::PropertyBitMask("Collision Groups", m_CollisionGroup, collisionGroups);

		UI::TextWithSeparator("Interacting Collision Groups", thickness, "Collision groups it can interact with");
		bRegenerate |= UI::PropertyBitMask("Interacting Collision Groups", m_InteractingCollisionGroup, collisionGroups);

		UI::EndPropertyGrid();

		ImGui::Separator();
		if (UI::Property("Simulate", bSimulate))
		{
			OnSimulateRagdollChanged();
		}

		ImGui::Separator();
		if (ImGui::Button("Regenerate") || bRegenerate)
		{
			auto& mesh = m_Asset->GetMesh();
			mesh->SetRagdollMaxTwist(m_Twist);
			mesh->SetRagdollMaxSwing(m_Swing);
			mesh->SetCollisionDetectionType(m_CollisionDetection);
			mesh->SetCollisionGroup(CollisionGroup(m_CollisionGroup));
			mesh->SetInteractingCollisionGroup(CollisionGroup(m_InteractingCollisionGroup));
			mesh->RegenerateRagdollData(m_MinRagdollBoneSize);
			bChanged = true;
		}

		ImGui::SameLine();

		if (ImGui::Button("Reset user settings"))
		{
			m_Asset->GetMesh()->ResetUserRagdollSettings();
			bChanged = true;
		}

		ImGui::EndTabItem();
		return bChanged;
	}
	
	void SkeletalMeshAssetEditor::UpdateGuizmo()
	{
		if (m_OpenedTab == OpenedTabType::Skeletal && m_SelectedBone)
		{
			const bool bEnableModification = m_SelectedBone->bVirtualBone;
			Transform boneTransform = Math::DecomposeTransformMatrix(Math::ToTransformMatrix(GetBoneWorldTransform(m_SelectedBoneName)));
			if (DrawGuizmo(boneTransform, bEnableModification, false))
			{
				// We need to remove parent's transform
				const glm::mat4 parentTr = Math::ToTransformMatrix(GetBoneWorldTransform(m_SelectedBoneParentName));
				m_SelectedBone->Transformation = glm::inverse(parentTr) * Math::ToTransformMatrix(boneTransform);
				bGuizmoChanged = true;
			}
		}
		else if (m_OpenedTab == OpenedTabType::Ragdoll && m_SelectedRagdollBone)
		{
			const bool bEnableModification = true;
			const glm::vec3 worldLocation = GetSelectedRagdollBoneWorldTransform().Location;
			Transform boneTransform = m_SelectedRagdollBone->Settings.UserOffset;
			const glm::vec3 origOffsetLocation = boneTransform.Location;
			boneTransform.Location = worldLocation; // We wanna draw guizmo in WS
			if (DrawGuizmo(boneTransform, bEnableModification, false))
			{
				const glm::vec3 diff = boneTransform.Location - worldLocation;
				boneTransform.Location = origOffsetLocation + diff; // Back to local
				m_SelectedRagdollBone->Settings.UserOffset = boneTransform;
				bGuizmoChanged = true;
			}
		}
	}
	
	Transform SkeletalMeshAssetEditor::GetSelectedRagdollBoneWorldTransform()
	{
		const bool bRuntime = bSimulate && m_OpenedTab == OpenedTabType::Ragdoll;
		Entity entity;

		if (bRuntime)
		{
			const auto& scene = GetCurrentScene();
			auto view = scene->GetAllEntitiesWith<SkeletalMeshComponent>();
			auto entt = view.front();
			entity = Entity(entt, scene.get());
		}
		else
		{
			entity = m_Entity;
		}

		Transform transform = entity.GetComponent<SkeletalMeshComponent>().GetRagdollBoneWorldTransform(m_SelectedRagdollBoneName);
		transform.Scale3D = m_SelectedRagdollBone->Settings.UserOffset.Scale3D; // Originally, bones don't have scale, so we restore it
		return transform;
	}

	Transform SkeletalMeshAssetEditor::GetBoneWorldTransform(const std::string& name)
	{
		const bool bRuntime = bSimulate && m_OpenedTab == OpenedTabType::Ragdoll;
		if (bRuntime)
		{
			const auto& scene = GetCurrentScene();
			auto view = scene->GetAllEntitiesWith<SkeletalMeshComponent>();
			auto entt = view.front();
			Entity entity = Entity(entt, scene.get());
			return entity.GetComponent<SkeletalMeshComponent>().GetBoneWorldTransform(name);
		}
		else
		{
			return m_Entity.GetComponent<SkeletalMeshComponent>().GetBoneWorldTransform(name);
		}
	}
	
	void SkeletalMeshAssetEditor::CreatePlane()
	{
		const auto& cube = AssetManager::GetPreviewCube();
		Entity plane = GetCurrentScene()->CreateEntity("SkeletalMeshAssetEditor_Plane1");
		m_PlaneEntityGUID = plane.GetGUID();
		plane.AddComponent<StaticMeshComponent>().SetMeshAsset(cube);
		plane.AddComponent<BoxColliderComponent>();

		const auto& mesh = m_Asset->GetMesh();
		const auto& aabb = mesh->GetAABB();
		const glm::vec3 center = aabb.Center();
		const glm::vec3 planeLocation = glm::vec3(center.x, aabb.Min.y, center.z); // Place the plane under the mesh

		constexpr static glm::vec3 planeScale = glm::vec3(100.f, 0.05f, 100.f);
		const glm::vec3 extent = glm::abs(aabb.Extents()) * 0.5f;
		const glm::vec3 meshScale = glm::vec3(extent.x, 1.f, extent.z); // To make the floor bigger than the mesh
		Transform tr;
		tr.Location = planeLocation;
		tr.Scale3D = planeScale * meshScale;
		plane.SetWorldTransform(tr);
	}

	void SkeletalMeshAssetEditor::DeletePlane()
	{
		const auto& scene = GetCurrentScene();
		if (Entity plane = scene->GetEntityByGUID(m_PlaneEntityGUID))
		{
			scene->DestroyEntity(plane);
			m_PlaneEntityGUID = GUID(0, 0);
		}
	}
	
	void SkeletalMeshAssetEditor::OnSimulateRagdollChanged()
	{
		m_Asset->OnModified(); // Reset ragdoll
		SetSimulationEnabled(bSimulate);
	}
	
	void SkeletalMeshAssetEditor::OnBoneNodeDeletion(const BoneNode& node)
	{
		if (auto it = m_AttachedToBonesMeshes.find(node.Name); it != m_AttachedToBonesMeshes.end())
		{
			GetCurrentScene()->DestroyEntity(it->second.Entity);
			m_AttachedToBonesMeshes.erase(it);
		}
		if (auto it = m_AttachedToBonesColliders.find(node.Name); it != m_AttachedToBonesColliders.end())
		{
			GetCurrentScene()->DestroyEntity(it->second.Entity);
			m_AttachedToBonesColliders.erase(it);
		}

		for (const auto& child : node.Children)
			OnBoneNodeDeletion(child);
	}
}
