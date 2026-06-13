#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Physics/PhysicsUtils.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/AI/NavigationDebugDraw.h"

namespace Eagle
{
	namespace Utils
	{
		constexpr uint32_t s_SphereLinesCount = 24;
		constexpr float s_2PI = 2.f * glm::pi<float>();

		void DrawSphere(std::vector<RendererLine>& buffer, const glm::vec3& center, const glm::vec3& color, float radius)
		{
			for (uint32_t i = 0; i < s_SphereLinesCount; ++i)
			{
				const float angle1 = (float(i) / s_SphereLinesCount) * s_2PI;
				const float angle2 = (float(i + 1) / s_SphereLinesCount) * s_2PI;
				const float cosAngle1 = glm::cos(angle1);
				const float cosAngle2 = glm::cos(angle2);
				const float sinAngle1 = glm::sin(angle1);
				const float sinAngle2 = glm::sin(angle2);
				constexpr float cos45 = 0.707106f;
				constexpr float cosMinus45 = -0.707106f;

				auto& line = buffer.emplace_back();
				line.Start.Location = center + radius * glm::vec3(cosAngle1, sinAngle1, 0.f);
				line.End.Location = center + radius * glm::vec3(cosAngle2, sinAngle2, 0.f);
				line.Start.Color = color;
				line.End.Color = color;

				auto& line2 = buffer.emplace_back();
				line2.Start.Location = center + radius * glm::vec3(0.f, cosAngle1, sinAngle1);
				line2.End.Location = center + radius * glm::vec3(0.f, cosAngle2, sinAngle2);
				line2.Start.Color = color;
				line2.End.Color = color;

				auto& line3 = buffer.emplace_back();
				line3.Start.Location = center + radius * glm::vec3(cos45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line3.End.Location = center + radius * glm::vec3(cos45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line3.Start.Color = color;
				line3.End.Color = color;

				auto& line4 = buffer.emplace_back();
				line4.Start.Location = center + radius * glm::vec3(cosMinus45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line4.End.Location = center + radius * glm::vec3(cosMinus45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line4.Start.Color = color;
				line4.End.Color = color;
			}
		}

		void DrawBones_Internal(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, const glm::mat4& baseTransform)
		{
			glm::mat4 tr;
			if (auto it = currentPose.Bones.find(node.Name); it != currentPose.Bones.end())
			{
				const auto& bone = it->second;
				const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
				tr = baseTransform * boneTransform;
			}
			else
				tr = baseTransform * node.Transformation;

			const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(tr).Location;
			for (const auto& child : node.Children)
			{
				auto& line = buffer.emplace_back();
				line.Start.Location = parentLocation;
				line.Start.Color = glm::vec3(0, 1, 0);
				
				const glm::vec3 location = Math::DecomposeTransformMatrix(tr * child.Transformation).Location;
				line.End.Location = location;
				line.End.Color = glm::vec3(1, 1, 0);
			}

			for (const auto& child : node.Children)
				DrawBones_Internal(buffer, child, currentPose, tr);
		}

		void DrawRagdollBones_Internal(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, const glm::mat4& worldTransform, const glm::mat4& baseTransform)
		{
			glm::mat4 tr;
			if (auto it = currentPose.Bones.find(node.Name); it != currentPose.Bones.end())
			{
				const auto& bone = it->second;
				tr = Math::ToTransformMatrix(bone); // Ragdoll is already a global transform
			}
			else
				tr = baseTransform * node.Transformation;

			const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(worldTransform * tr).Location;
			for (const auto& child : node.Children)
			{
				auto& line = buffer.emplace_back();
				line.Start.Location = parentLocation;
				line.Start.Color = glm::vec3(0, 1, 0);

				const glm::vec3 location = Math::DecomposeTransformMatrix(worldTransform * (tr * child.Transformation)).Location;
				line.End.Location = location;
				line.End.Color = glm::vec3(1, 1, 0);
			}

			for (const auto& child : node.Children)
				DrawRagdollBones_Internal(buffer, child, currentPose, worldTransform, tr);
		}

		void DrawBones(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, bool bRagdoll, const glm::mat4& baseTransform)
		{
			if (bRagdoll)
				DrawRagdollBones_Internal(buffer, node, currentPose, baseTransform, baseTransform);
			else
				DrawBones_Internal(buffer, node, currentPose, baseTransform);
		}
	
		void DrawBox(std::vector<RendererLine>& buffer, AABB aabb, const Transform& worldTr, const glm::vec3& color = glm::vec3(0, 1, 0))
		{
			const glm::mat4 trMat = Math::ToTransformMatrix(worldTr);
			const size_t startIdx = buffer.size();

			for (glm::length_t i = 0; i < aabb.Min.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;

				line.End.Location = aabb.Min;
				line.End.Location[i] = aabb.Max[i];
			}

			for (glm::length_t i = 0; i < aabb.Max.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Max;

				line.End.Location = aabb.Max;
				line.End.Location[i] = aabb.Min[i];
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}
		
			for (size_t i = startIdx; i < buffer.size(); ++i)
			{
				auto& line = buffer[i];
				line.Start.Location = trMat * glm::vec4(line.Start.Location, 1.f);
				line.End.Location = trMat * glm::vec4(line.End.Location, 1.f);
			}
		}

		void DrawAABB(std::vector<RendererLine>& buffer, AABB aabb, const Transform& worldTr, const glm::vec3& color = glm::vec3(0, 1, 0))
		{
			const glm::mat4 trMat = Math::ToTransformMatrix(worldTr);
			const size_t startIdx = buffer.size();

			aabb.Transform(trMat);

			for (glm::length_t i = 0; i < aabb.Min.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;

				line.End.Location = aabb.Min;
				line.End.Location[i] = aabb.Max[i];
			}

			for (glm::length_t i = 0; i < aabb.Max.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Max;

				line.End.Location = aabb.Max;
				line.End.Location[i] = aabb.Min[i];
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}
		}

		void DrawFrustum(std::vector<RendererLine>& lines, const Camera& camera, const Transform& tr, float aspect, const glm::vec3& color = glm::vec3(0, 1, 0))
		{
			auto AddLine = [](std::vector<RendererLine>& lines,
				const glm::vec3& a,
				const glm::vec3& b,
				const glm::vec3& color)
			{
				RendererLine l;
				l.Start.Location = a;
				l.Start.Color = color;
				l.End.Location = b;
				l.End.Color = color;
				lines.push_back(l);
			};

			const float fovY = camera.GetPerspectiveVerticalFOV();
			const float znear = camera.GetPerspectiveNearClip();
			const float zfar = camera.GetPerspectiveFarClip();

			const glm::vec3& pos = tr.Location;
			const glm::vec3 forward = Math::GetForwardVector(tr.Rotation);
			const glm::vec3 up = Math::GetUpVector(tr.Rotation);
			const glm::vec3 right = Math::GetRightVector(tr.Rotation);

			float tanHalfFov = tanf(fovY * 0.5f);

			float nearH = 2.0f * tanHalfFov * znear;
			float nearW = nearH * aspect;

			float farH = 2.0f * tanHalfFov * zfar;
			float farW = farH * aspect;

			glm::vec3 nc = pos + forward * znear;
			glm::vec3 fc = pos + forward * zfar;

			// Near corners
			glm::vec3 ntl = nc + (up * nearH * 0.5f) - (right * nearW * 0.5f);
			glm::vec3 ntr = nc + (up * nearH * 0.5f) + (right * nearW * 0.5f);
			glm::vec3 nbl = nc - (up * nearH * 0.5f) - (right * nearW * 0.5f);
			glm::vec3 nbr = nc - (up * nearH * 0.5f) + (right * nearW * 0.5f);

			// Far corners
			glm::vec3 ftl = fc + (up * farH * 0.5f) - (right * farW * 0.5f);
			glm::vec3 ftr = fc + (up * farH * 0.5f) + (right * farW * 0.5f);
			glm::vec3 fbl = fc - (up * farH * 0.5f) - (right * farW * 0.5f);
			glm::vec3 fbr = fc - (up * farH * 0.5f) + (right * farW * 0.5f);

			// Colors
			const glm::vec3 edgeColor = color; // (1, 1, 0);
			const glm::vec3 nearColor = color; // (0, 1, 1);
			const glm::vec3 farColor = color; // (0, 0.5f, 1);
			const glm::vec3 sliceColor = color; // (1, 0.5f, 0);
			const glm::vec3 diagColor = color; // (1, 0, 0);

			// Near
			AddLine(lines, ntl, ntr, edgeColor);
			AddLine(lines, ntr, nbr, edgeColor);
			AddLine(lines, nbr, nbl, edgeColor);
			AddLine(lines, nbl, ntl, edgeColor);

			// Far
			AddLine(lines, ftl, ftr, edgeColor);
			AddLine(lines, ftr, fbr, edgeColor);
			AddLine(lines, fbr, fbl, edgeColor);
			AddLine(lines, fbl, ftl, edgeColor);

			// Connections
			AddLine(lines, ntl, ftl, edgeColor);
			AddLine(lines, ntr, ftr, edgeColor);
			AddLine(lines, nbl, fbl, edgeColor);
			AddLine(lines, nbr, fbr, edgeColor);

			// Diagonals (orientation)
			AddLine(lines, ntl, nbr, diagColor);
			AddLine(lines, ntr, nbl, diagColor);
			AddLine(lines, ftl, fbr, diagColor);
			AddLine(lines, ftr, fbl, diagColor);

			// Near plane grid
			const int gridSteps = 1;
			for (int i = 1; i < gridSteps; i++)
			{
				float t = i / (float)gridSteps;
			
				glm::vec3 left = glm::mix(nbl, ntl, t);
				glm::vec3 right = glm::mix(nbr, ntr, t);
				AddLine(lines, left, right, nearColor);
			
				glm::vec3 bottom = glm::mix(nbl, nbr, t);
				glm::vec3 top = glm::mix(ntl, ntr, t);
				AddLine(lines, bottom, top, nearColor);
			}

			// Far plane grid
			for (int i = 1; i < gridSteps; i++)
			{
				float t = i / (float)gridSteps;
			
				glm::vec3 left = glm::mix(fbl, ftl, t);
				glm::vec3 right = glm::mix(fbr, ftr, t);
				AddLine(lines, left, right, farColor);
			
				glm::vec3 bottom = glm::mix(fbl, fbr, t);
				glm::vec3 top = glm::mix(ftl, ftr, t);
				AddLine(lines, bottom, top, farColor);
			}

			// Depth slices
			const int depthSlices = 3;
			for (int i = 1; i <= depthSlices; i++)
			{
				float t = i / (float)(depthSlices + 1);
				float z = glm::mix(znear, zfar, t);

				float h = 2.0f * tanHalfFov * z;
				float w = h * aspect;

				glm::vec3 center = pos + forward * z;

				glm::vec3 tl = center + (up * h * 0.5f) - (right * w * 0.5f);
				glm::vec3 tr = center + (up * h * 0.5f) + (right * w * 0.5f);
				glm::vec3 bl = center - (up * h * 0.5f) - (right * w * 0.5f);
				glm::vec3 br = center - (up * h * 0.5f) + (right * w * 0.5f);

				AddLine(lines, tl, tr, sliceColor);
				AddLine(lines, tr, br, sliceColor);
				AddLine(lines, br, bl, sliceColor);
				AddLine(lines, bl, tl, sliceColor);
			}
		}

		template <typename Comp>
		void InvalidateCollisionGroups(entt::registry& registry, uint32_t validMasks)
		{
			auto view = registry.view<Comp>();
			for (auto entityID : view)
			{
				auto& comp = view.get<Comp>(entityID);
				comp.SetCollisionGroup(CollisionGroup(uint32_t(comp.GetCollisionGroup()) & validMasks));
				comp.SetInteractingCollisionGroup(CollisionGroup(uint32_t(comp.GetInteractingCollisionGroup()) & validMasks));
			}
		}
	}

	Ref<Scene> Scene::s_CurrentScene;
	static Ref<PhysicsActor> s_NullPhysicsActor;

	static std::unordered_map<GUID, std::function<void(const Ref<Scene>&)>> s_OnSceneOpenedCallbacks;

	template<typename T>
	static void SceneAddAndCopyComponent(Scene* destScene, entt::registry& destRegistry, entt::registry& srcRegistry, const std::unordered_map<entt::entity, entt::entity>& createdEntities)
	{
		auto entities = srcRegistry.view<T>();
		for (auto srcEntity : entities)
		{
			auto& srcComponent = srcRegistry.get<T>(srcEntity);
			Entity destEntity(createdEntities.at(srcEntity), destScene);
			if (destEntity.HasComponent<T>())
			{
				T& comp = destEntity.GetComponent<T>();
				comp = srcComponent;
			}
			else
			{
				T& comp = destEntity.AddComponent<T>();
				comp = srcComponent;
			}
		}
	}

	template<typename T>
	static void EntityCopyComponent(const Entity& src, Entity& destination)
	{
		if (src.HasComponent<T>())
		{
			if (!destination.HasComponent<T>())
				destination.AddComponent<T>();

			destination.GetComponent<T>() = src.GetComponent<T>();
		}
	}

	Scene::Scene()
		: m_DebugName("Empty")
	{
		m_RuntimePhysicsScene = MakeRef<PhysicsScene>(PhysicsSettings());
		m_PhysicsScene = m_RuntimePhysicsScene;
		SetupOnAppAssemblyReloadedCallback();
	}

	Scene::Scene(const std::string& debugName, const Ref<SceneRenderer>& sceneRenderer, bool bRuntime)
		: m_DebugName(debugName)
	{
		if (sceneRenderer)
		{
			m_SceneRenderer = sceneRenderer;
		}
		else
		{
			m_SceneRenderer = MakeRef<SceneRenderer>(glm::uvec2{ m_ViewportWidth, m_ViewportHeight });
		}
		OnViewportResize(m_SceneRenderer->GetViewportSize().x, m_SceneRenderer->GetViewportSize().y);
		SetUseSkyAsBackground(m_bUseSkyAsBackground);
		SetRenderSkybox(m_bRenderSkybox);
		SetSkyboxEnabled(m_bSkyboxEnabled);
		SetSkyboxIntensity(m_SkyboxIntensity);
		SetSkybox(m_Sky);
		ConnectSignals();

		m_RuntimePhysicsScene = MakeRef<PhysicsScene>(m_RuntimePhysicsSettings);
		if (bRuntime)
		{
			m_PhysicsScene = m_RuntimePhysicsScene;
		}
		else
		{
			PhysicsSettings editorSettings;
			editorSettings.UpdateRate = 30u;
			editorSettings.Gravity = glm::vec3{ 0.f };
			editorSettings.bDebugOnPlay = false;
			editorSettings.bEditorScene = true;
			m_PhysicsScene = MakeRef<PhysicsScene>(editorSettings);
		}
		SetupOnAppAssemblyReloadedCallback();
	}

	Scene::Scene(const Ref<Scene>& other, const std::string& debugName)
	: bCanUpdateEditorCamera(other->bCanUpdateEditorCamera)
	, m_RuntimePhysicsScene(other->m_RuntimePhysicsScene)
	, m_PhysicsScene(other->m_RuntimePhysicsScene)
	, EditorCamera(other->EditorCamera)
	, m_EntitiesToDestroy(other->m_EntitiesToDestroy)
	, m_ViewportWidth(other->m_ViewportWidth)
	, m_ViewportHeight(other->m_ViewportHeight)
	, m_DebugName(debugName)
	, m_RuntimePhysicsSettings(other->m_RuntimePhysicsSettings)
	, bDrawMiscellaneous(other->bDrawMiscellaneous)
	, bDrawNavMesh(other->bDrawNavMesh)
	, bDrawMeshAABBs(other->bDrawMeshAABBs)
	, bDrawBones(other->bDrawBones)
	, m_Cubemap(other->m_Cubemap)
	, m_Sky(other->m_Sky)
	, m_SkyboxIntensity(other->m_SkyboxIntensity)
	, m_bSkyboxEnabled(other->m_bSkyboxEnabled)
	, m_bRenderSkybox(other->m_bRenderSkybox)
	, m_bUseSkyAsBackground(other->m_bUseSkyAsBackground)
	, m_CurrentNavMeshEntityGUID(other->m_CurrentNavMeshEntityGUID)
	{
		// Reuse renderer so that we don't allocate additional GPU resources
		m_SceneRenderer = other->m_SceneRenderer;
		OnViewportResize(m_SceneRenderer->GetViewportSize().x, m_SceneRenderer->GetViewportSize().y);
		SetUseSkyAsBackground(m_bUseSkyAsBackground);
		SetRenderSkybox(m_bRenderSkybox);
		SetSkyboxEnabled(m_bSkyboxEnabled);
		SetSkyboxIntensity(m_SkyboxIntensity);
		SetSkybox(m_Sky);

		std::unordered_map<entt::entity, entt::entity> createdEntities;
		createdEntities.reserve(other->GetEntitiesCount());
		for (auto entt : other->m_Registry.view<TransformComponent>())
		{
			const std::string& sceneName = other->m_Registry.get<EntitySceneNameComponent>(entt).Name;
			const GUID& guid  = other->m_Registry.get<IDComponent>(entt).ID;
			Entity entity = CreateEntityWithGUID(guid, sceneName);
			createdEntities[entt] = entity.GetEnttID();
		}

		SceneAddAndCopyComponent<TransformComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<TagComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<OwnershipComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<EntityAssetComponent>(this, m_Registry, other->m_Registry, createdEntities);

		SceneAddAndCopyComponent<NativeScriptComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<ScriptComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<PointLightComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<DirectionalLightComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SpotLightComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SpriteComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<StaticMeshComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SkeletalMeshComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<BillboardComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<CameraComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<RigidBodyComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<BoxColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SphereColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<CapsuleColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<MeshColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<AudioComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<ReverbComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<TextComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<Text2DComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<Image2DComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<ParticleSystemComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<DecalComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<NavigationCrowdAgentComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<NavigationMeshComponent>(this, m_Registry, other->m_Registry, createdEntities);

		for (auto entt : m_Registry.view<RigidBodyComponent>())
		{
			Entity entity{entt, this};
			if (!entity.HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>())
				m_PhysicsScene->CreatePhysicsActor(entity);
		}

		ConnectSignals();
		SetupOnAppAssemblyReloadedCallback();
		m_DirtyFlags.SetEverythingDirty(true);
	}

	Scene::~Scene()
	{
		if (IsPlaying())
			OnRuntimeStop();

		m_RuntimeCameraHolder.reset();

		DestroyScripts();

		m_CurrentNavMesh.reset();
		m_PhysicsScene.reset();
		m_RuntimePhysicsScene.reset();
		m_SpawnedSounds.clear();
		m_Registry.clear();

		ScriptEngine::RemoveOnAppAssemblyReloadedCallback(m_GUID);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithGUID(GUID(), name);
	}

	Entity Scene::CreateEntityWithGUID(GUID guid, const std::string& name)
	{
		const std::string& sceneName = name.empty() ? "Unnamed Entity" : name;
		Entity entity = Entity(m_Registry.create(), this);
		entity.AddComponent<IDComponent>(guid);
		entity.AddComponent<EntitySceneNameComponent>(sceneName);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<TagComponent>();
		entity.AddComponent<OwnershipComponent>();

		m_AliveEntities[guid] = entity;

		return entity;
	}

	Entity Scene::CreateFromEntity(const Entity& source, bool bCopyGUID)
	{
		const GUID guid = bCopyGUID ? source.GetComponent<IDComponent>().ID : GUID{};
		Entity result = CreateEntityWithGUID(guid, source.GetComponent<EntitySceneNameComponent>().Name);
		EntityCopyComponent<TransformComponent>(source, result); //Copying TransformComponent to set childrens transform correctly

		// Recreating Ownership component
		const auto& srcChildren = source.GetChildren();
		for (auto& child : srcChildren)
		{
			// Don't copy entities that are about to be destroyed
			if (child.GetScene()->IsPendingDestroy(child))
				continue;

			Entity myChild = CreateFromEntity(child, bCopyGUID);
			myChild.SetParent(result);
		}

		CopyComponents(source, result);

		return result;
	}

	void Scene::DestroyEntity(Entity entity, bool bDestroyChildren)
	{
		if (!entity)
			return;

		if (bIsPlaying)
		{
			if (entity.HasComponent<NativeScriptComponent>())
			{
				auto& nsc = entity.GetComponent<NativeScriptComponent>();
				nsc.Destroy();
			}

			if (entity.HasComponent<ScriptComponent>())
				ScriptEngine::OnDestroyEntity(entity);
		}

		m_EntitiesToDestroy.emplace_back(entity, bDestroyChildren);

		// EG_CORE_TRACE("Destroyed Entity: {}", entity.GetComponent<EntitySceneNameComponent>().Name);
	}

	void Scene::DestroyEntityImmediately(Entity entity, bool bDestroyChildren)
	{
		ScriptEngine::RemoveEntityScript(entity);
		auto& actor = entity.GetPhysicsActor();
		if (actor)
			m_PhysicsScene->RemovePhysicsActor(actor);

		auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
		std::vector<Entity> children = ownershipComponent.Children; // Copy, otherwise it'll be modified when we iterate over it
		Entity myParent = ownershipComponent.EntityParent;
		entity.SetParent(Entity::Null);

		if (bDestroyChildren)
		{
			for (size_t i = 0; i < children.size(); ++i)
				DestroyEntityImmediately(children[i], bDestroyChildren);
		}
		else
		{
			for (size_t i = 0; i < children.size(); ++i)
				children[i].SetParent(myParent);
		}

		m_AliveEntities.erase(entity.GetGUID());
		m_Registry.destroy(entity.GetEnttID());
	}

	void Scene::OnUpdate(Timestep ts, bool bRender, bool bForceAnimationsUpdate)
	{
		if (bIsPlaying)
			OnUpdateRuntime(ts, bRender, bForceAnimationsUpdate);
		else
			OnUpdateEditor(ts, bRender, bForceAnimationsUpdate);
	}

	void Scene::OpenScene(const Ref<AssetScene>& sceneAsset, bool bReuseCurrentSceneRenderer, bool bRuntime)
	{
		auto func = [path = sceneAsset ? sceneAsset->GetPath() : "", bReuseCurrentSceneRenderer, bRuntime]()
		{
			ComponentsNotificationSystem::Reset();
			ScriptEngine::Reset();
			RenderManager::Wait();
			Ref<Scene> scene = MakeRef<Scene>(Utils::AsString(path), (bReuseCurrentSceneRenderer && s_CurrentScene) ? s_CurrentScene->GetSceneRenderer() : nullptr, bRuntime);
			scene->SetSkybox(SkySettings{});
			scene->SetSkybox(nullptr);
			scene->SetSkyboxIntensity(1.f);

			if (Application::Get().IsGame())
			{
				Ref<ScopedDataBuffer> sceneData;
				if (AssetManager::GetRuntimeAssetData(path, &sceneData))
				{
					AssetManager::ResetGameAssets();
					AssetManager::ResetRuntimeAsset();
					SceneSerializer::Deserialize(scene, sceneData->GetDataBuffer());
					OnSceneOpened(scene);
				}
				else
					EG_CORE_ERROR("Failed to open the scene. The asset is not found: {}", path);
			}
			else
			{
				if (std::filesystem::exists(path))
				{
					SceneSerializer::Deserialize(scene, path);
				}
				AssetManager::ResetRuntimeAsset();
				OnSceneOpened(scene);
			}
		};

		Application::Get().CallNextFrame(func);
	}

	void Scene::SetSkybox(const Ref<AssetTextureCube>& cubemap)
	{
		m_Cubemap = cubemap;
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkybox(m_Cubemap);
	}

	void Scene::SetSkybox(const SkySettings& sky)
	{
		m_Sky = sky;
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkybox(m_Sky);
	}

	void Scene::SetSkyboxIntensity(float intensity)
	{
		m_SkyboxIntensity = glm::max(0.f, intensity);
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkyboxIntensity(intensity);
	}

	void Scene::SetSkyboxEnabled(bool bEnabled)
	{
		m_bSkyboxEnabled = bEnabled;
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkyboxEnabled(m_bSkyboxEnabled);
	}

	void Scene::SetRenderSkybox(bool bEnabled)
	{
		m_bRenderSkybox = bEnabled;
		if (m_SceneRenderer)
			m_SceneRenderer->SetRenderSkybox(m_bRenderSkybox);
	}

	void Scene::SetUseSkyAsBackground(bool value)
	{
		m_bUseSkyAsBackground = value;
		if (m_SceneRenderer)
			m_SceneRenderer->SetUseSkyAsBackground(m_bUseSkyAsBackground);
	}

	void Scene::BuildNavMesh(NavigationMeshComponent* navMesh)
	{
		std::vector<BaseColliderComponent*> obstacleColliders;
		obstacleColliders.reserve(100u);
		// Collect obstacle colliders
		{
			// Box colliders
			{
				auto view = m_Registry.view<BoxColliderComponent>();
				for (auto entity : view)
				{
					auto& component = view.get<BoxColliderComponent>(entity);
					if (component.IsObstacle())
						obstacleColliders.push_back(&component);
				}
			}
			// Sphere colliders
			{
				auto view = m_Registry.view<SphereColliderComponent>();
				for (auto entity : view)
				{
					auto& component = view.get<SphereColliderComponent>(entity);
					if (component.IsObstacle())
						obstacleColliders.push_back(&component);
				}
			}
			// Capsule colliders
			{
				auto view = m_Registry.view<CapsuleColliderComponent>();
				for (auto entity : view)
				{
					auto& component = view.get<CapsuleColliderComponent>(entity);
					if (component.IsObstacle())
						obstacleColliders.push_back(&component);
				}
			}
		}

		// Go through all colliders and delete obstacles
		for (BaseColliderComponent* collider : obstacleColliders)
			collider->SetIsObstacle(false);

		auto agentComponents = m_Registry.view<NavigationCrowdAgentComponent>();

		// Go through all agents and delete them
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.RemoveAgent();
		}

		// Destroy NavMeshes
		{
			m_CurrentNavMesh.reset();
			auto view = m_Registry.view<NavigationMeshComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<NavigationMeshComponent>(entity);
				component.DestroyNavMesh();
			}
		}

		// Build a new nav mesh
		if (navMesh)
		{
			navMesh->Build();
			m_CurrentNavMesh = navMesh->GetNavMesh();
			m_CurrentNavMeshEntityGUID = navMesh->Parent.GetGUID();
		}
		else
		{
			m_CurrentNavMeshEntityGUID = GUID(0, 0);
		}

		// Go through all agents and create them back
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.CreateAgent(component.Parent.GetWorldLocation());
		}

		// Go through all colliders and generate obstacles back
		for (BaseColliderComponent* collider : obstacleColliders)
			collider->SetIsObstacle(true);
	}

	void Scene::RebuildNavMesh()
	{
		if (m_CurrentNavMeshEntityGUID.IsNull())
			return;

		Entity entity = GetEntityByGUID(m_CurrentNavMeshEntityGUID);
		if (!entity)
			return;

		if (!entity.HasComponent<NavigationMeshComponent>())
			return;

		auto& comp = entity.GetComponent<NavigationMeshComponent>();
		if (comp.bAutoRebuild)
			BuildNavMesh(&comp);
	}

	void Scene::BuildCrowd(const AINavigation::CrowdSettings& settings)
	{
		if (!m_CurrentNavMesh)
			return;

		auto agentComponents = m_Registry.view<NavigationCrowdAgentComponent>();

		// Go through all agents and delete them
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.RemoveAgent();
		}

		// Recreate crowd system
		m_CurrentNavMesh->GetCrowd().SetSettings(settings);

		// Go through all agents and create them back
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.CreateAgent(component.Parent.GetWorldLocation());
		}
	}

	GUID Scene::AddOnSceneOpenedCallback(const std::function<void(const Ref<Scene>&)>& func)
	{
		GUID id{};
		s_OnSceneOpenedCallbacks[id] = func;
		return id;
	}

	void Scene::RemoveOnSceneOpenedCallback(GUID id)
	{
		s_OnSceneOpenedCallbacks.erase(id);
	}

	void Scene::OnSceneOpened(const Ref<Scene>& scene)
	{
		for (auto& [id, func] : s_OnSceneOpenedCallbacks)
			func(scene);
	}

	void Scene::OnUpdateEditor(Timestep ts, bool bRender, bool bForceAnimationsUpdate)
	{
		DestroyPendingEntities();

		EditorCamera.OnUpdate(ts, bCanUpdateEditorCamera);

		GatherSkeletalMeshes();
		UpdateAnimations(ts, !bForceAnimationsUpdate, false);
		UpdateNavMesh(ts);
		m_PhysicsScene->Simulate(ts, false);
		if (bRender)
			RenderScene(ts, false);
	}

	void Scene::OnUpdateRuntime(Timestep ts, bool bRender, bool bForceAnimationsUpdate)
	{	
		DestroyPendingEntities();

		m_RuntimeCamera = FindOrCreateRuntimeCamera();
		if (!m_RuntimeCamera->FixedAspectRatio)
		{
			if (m_RuntimeCamera->Camera.GetViewportWidth() != m_ViewportWidth || m_RuntimeCamera->Camera.GetViewportHeight() != m_ViewportHeight)
				m_RuntimeCamera->Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		}

		GatherSkeletalMeshes();
		UpdateAnimations(ts, false, true);
		UpdateNavMesh(ts);
		SyncCrowdAgents();
		m_PhysicsScene->Simulate(ts, true);
		UpdateScripts(ts);
		AudioEngine::SetListenerData(m_RuntimeCamera->GetWorldTransform().Location, m_RuntimeCamera->GetForwardVector(), m_RuntimeCamera->GetUpVector());
		if (bRender)
			RenderScene(ts, true);
	}

	void Scene::UpdateNavMesh(Timestep ts)
	{
		if (!m_CurrentNavMesh)
			return;

		EG_CPU_TIMING_SCOPED("Scene. Update NavMesh");
		m_CurrentNavMesh->Update(ts);
	}

	void Scene::SyncCrowdAgents()
	{
		if (!m_CurrentNavMesh)
			return;

		auto view = m_Registry.view<NavigationCrowdAgentComponent>();
		for (auto& e : view)
		{
			Entity entity = Entity(e, this);
			auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
			glm::vec3 location;
			if (component.GetLocation(&location))
			{
				entity.SetWorldLocation(location);
			}
		}
	}

	void Scene::SetupOnAppAssemblyReloadedCallback()
	{
		ScriptEngine::AddOnAppAssemblyReloadedCallback(m_GUID, [this]()
		{
			// Update entity public fields
			auto view = GetAllEntitiesWith<ScriptComponent>();
			for (auto entityID : view)
			{
				Entity entity{ entityID, this };
				ScriptEngine::UpdateEntityPublicFields(entity);
			}
		});
	}

	void Scene::GatherLightsInfo()
	{
		EG_CPU_TIMING_SCOPED("Scene. Gather Lights Info");

		if (m_DirtyFlags.bPointLightsDirty)
		{
			auto view = m_Registry.view<PointLightComponent>();
			m_PointLights.clear();

			for (auto entity : view)
			{
				auto& component = view.get<PointLightComponent>(entity);
				if (component.DoesAffectWorld())
					m_PointLights.push_back(&component);
			}
		}

		if (m_DirtyFlags.bDirLightsDirty)
		{
			m_DirectionalLights.clear();

			auto view = m_Registry.view<DirectionalLightComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<DirectionalLightComponent>(entity);
				if (component.DoesAffectWorld())
				{
					m_DirectionalLights.push_back(&component);
				}
			}
		}

		if (m_DirtyFlags.bSpotLightsDirty)
		{
			auto view = m_Registry.view<SpotLightComponent>();
			m_SpotLights.clear();

			for (auto entity : view)
			{
				auto& component = view.get<SpotLightComponent>(entity);
				if (component.DoesAffectWorld())
					m_SpotLights.push_back(&component);
			}
		}
	}

	void Scene::GatherSkeletalMeshes()
	{
		if (bForceSkeletalMeshUpdateNextFrame)
		{
			m_DirtyFlags.bSkeletalMeshesDirty = true;
			bForceSkeletalMeshUpdateNextFrame = false;
		}
		if (m_DirtyFlags.bSkeletalMeshesDirty)
		{
			// TODO: Maybe update the list in callbacks? 
			m_SkeletalMeshes.clear();
			auto view = m_Registry.view<SkeletalMeshComponent>();
			for (auto entity : view)
			{
				auto& mesh = view.get<SkeletalMeshComponent>(entity);
				if (mesh.GetMeshAsset() && mesh.IsVisible())
					m_SkeletalMeshes.push_back(&mesh);
			}
		}
	}

	void Scene::DestroyPendingEntities()
	{
		EG_CPU_TIMING_SCOPED("Scene. Destroy Pending Entities");

		//Remove entities when a new frame begins
		for (auto& [entity, bDestroyChildren] : m_EntitiesToDestroy)
		{
			DestroyEntityImmediately(entity, bDestroyChildren);
		}
		m_EntitiesToDestroy.clear();
	}

	bool Scene::IsPendingDestroy(Entity entity) const
	{
		auto it = std::find_if(m_EntitiesToDestroy.begin(), m_EntitiesToDestroy.end(), [entity](const std::pair<Entity, bool>& val)
		{
			return val.first == entity;
		});
		return it != m_EntitiesToDestroy.end();
	}

	void Scene::UpdateScripts(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("Scene. Run Scripts");

		const bool bDirtyBefore = m_DirtyFlags.bSkeletalMeshesDirty;
		m_DirtyFlags.bSkeletalMeshesDirty = false;

		// C++ scripts
		{
			auto view = m_Registry.view<NativeScriptComponent>();

			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);
				nsc.OnUpdate(Entity{ entity, this }, ts);
			}
		}

		// C# scripts
		{
			auto view = m_Registry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::OnUpdateEntity(e, ts);
			}
		}

		// C# animation events
		{
			for (const auto& [entityID, events] : m_AnimationsToTrigger)
			{
				for (const auto& event : events)
				{
					Entity entity((entt::entity)entityID, this);
					entity.TriggerAnimationEvent(event.Name, event.Time);
				}
			}
			m_AnimationsToTrigger.clear();
		}

		bForceSkeletalMeshUpdateNextFrame = m_DirtyFlags.bSkeletalMeshesDirty;
		m_DirtyFlags.bSkeletalMeshesDirty = bDirtyBefore;
	}

	void Scene::UpdateAnimations(Timestep ts, bool bUseBasePose, bool bApplyRootMotion)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update animations");

		m_AnimationsToTrigger.clear();
		if (bUseBasePose)
		{
			m_AnimationTransforms = AnimationSystem::UpdateBasePose(m_SkeletalMeshes, ts);
		}
		else
		{
			m_AnimationTransforms = AnimationSystem::Update(m_SkeletalMeshes, ts, bApplyRootMotion, &m_AnimationsToTrigger);
		}

		m_SystemsToUpdateAnims.clear();
		for (const auto& entityID : m_SkeletalParticles)
		{
			entt::entity entity = (entt::entity)entityID;
			EG_CORE_ASSERT(m_Registry.valid(entity) && m_Registry.all_of<ParticleSystemComponent>(entity));
			m_SystemsToUpdateAnims.push_back(&m_Registry.get<ParticleSystemComponent>(entity));
		}
		m_SkeletalParticlesAnimationTransforms = AnimationSystem::Update(m_SystemsToUpdateAnims, ts, &m_AnimationsToTrigger);
	}

	CameraComponent* Scene::FindOrCreateRuntimeCamera()
	{
		EG_CPU_TIMING_SCOPED("Scene. Find or Create runtime camera");

		CameraComponent* camera = nullptr;
		// Looking for Primary Camera
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			// Ignore engine provided camera
			if (m_RuntimeCameraHolder && entity == m_RuntimeCameraHolder->GetEnttID())
				continue;

			auto& cameraComponent = view.get<CameraComponent>(entity);

			if (cameraComponent.Primary)
			{
				camera = &cameraComponent;
				break;
			}
		}

		// If didn't find camera, create one
		if (!camera)
		{
			if (!m_RuntimeCameraHolder)
			{
				//If user provided primary-camera doesn't exist, provide one and set its transform to match editor camera's transform
				m_RuntimeCameraHolder = MakeScope<Entity>(CreateEntity("EAGLE:RuntimeCamera"));
				m_RuntimeCameraHolder->AddComponent<NativeScriptComponent>().Bind<CameraController>();
				m_RuntimeCameraHolder->RemoveComponent<EntitySceneNameComponent>(); // Delete it so it doesn't show up in the Scene hierarchy

				auto& cameraComp = m_RuntimeCameraHolder->AddComponent<CameraComponent>();
				cameraComp.Camera = EditorCamera;
				cameraComp.Primary = true;
				cameraComp.SetWorldTransform(EditorCamera.GetTransform());
			}
			camera = &m_RuntimeCameraHolder->GetComponent<CameraComponent>();
		}

		return camera;
	}

	void Scene::RenderScene(Timestep ts, bool bRuntime)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update Scene");

		GatherLightsInfo();

		if (m_DirtyFlags.bStaticMeshesDirty)
		{
			auto view = m_Registry.view<StaticMeshComponent>();
			m_Meshes.clear();
			for (auto entity : view)
			{
				auto& mesh = view.get<StaticMeshComponent>(entity);
				if (mesh.GetMeshAsset() && mesh.IsVisible())
					m_Meshes.push_back(&mesh);
			}
		}
		if (m_DirtyFlags.bSpritesDirty)
		{
			auto view = m_Registry.view<SpriteComponent>();
			m_Sprites.clear();
			for (auto entity : view)
			{
				auto& sprite = view.get<SpriteComponent>(entity);
				if (sprite.IsVisible())
					m_Sprites.push_back(&sprite);
			}
		}
		if (m_DirtyFlags.bDecalsDirty)
		{
			auto view = m_Registry.view<DecalComponent>();
			m_Decals.clear();
			for (auto entity : view)
			{
				auto& decal = view.get<DecalComponent>(entity);
				if (decal.IsVisible())
					m_Decals.push_back(&decal);
			}
		}

		// If meshes are dirty, there's not point in updating specific transforms
		// Since meshes are going to be fully updated anyway
		if (m_DirtyFlags.bStaticMeshTransformsDirty && !m_DirtyFlags.bStaticMeshesDirty)
		{
			std::vector<const StaticMeshComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformStaticMeshes.size());
			for (auto entityID : m_DirtyTransformStaticMeshes)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<StaticMeshComponent>())
				{
					const auto& comp = e.GetComponent<StaticMeshComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateMeshesTransforms(dirtyComponents);
		}

		// Same for skeletals
		if (m_DirtyFlags.bSkeletalMeshTransformsDirty && !m_DirtyFlags.bSkeletalMeshesDirty)
		{
			std::vector<const SkeletalMeshComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformSkeletalMeshes.size());
			for (auto entityID : m_DirtyTransformSkeletalMeshes)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<SkeletalMeshComponent>())
				{
					const auto& comp = e.GetComponent<SkeletalMeshComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateSkeletalMeshesTransforms(dirtyComponents);
		}

		// Same for sprites
		if (m_DirtyFlags.bSpriteTransformsDirty && !m_DirtyFlags.bSpritesDirty)
		{
			std::vector<const SpriteComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformSprites.size());
			for (auto entityID : m_DirtyTransformSprites)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<SpriteComponent>())
				{
					const auto& comp = e.GetComponent<SpriteComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateSpritesTransforms(dirtyComponents);
		}

		// Same for decals
		if (m_DirtyFlags.bDecalTransformsDirty && !m_DirtyFlags.bDecalsDirty)
		{
			std::vector<const DecalComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformDecals.size());
			for (auto entityID : m_DirtyTransformDecals)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<DecalComponent>())
				{
					const auto& comp = e.GetComponent<DecalComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}
			m_SceneRenderer->UpdateDecalsTransforms(dirtyComponents);
		}

		// Same for texts
		if (m_DirtyFlags.bTextTransformsDirty && !m_DirtyFlags.bTextDirty)
		{
			std::vector<const TextComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformTexts.size());
			for (auto entityID : m_DirtyTransformTexts)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<TextComponent>())
				{
					const auto& comp = e.GetComponent<TextComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateTextsTransforms(dirtyComponents);
		}

		// Gather billboards
		{
			auto view = m_Registry.view<BillboardComponent>();
			m_Billboards.clear();
			for (auto entity : view)
			{
				auto& billboard = view.get<BillboardComponent>(entity);
				if (billboard.bVisible)
					m_Billboards.push_back(&billboard);
			}
		}

		auto& rb = m_PhysicsScene->GetRenderBuffer();
		const uint32_t debugCollisionsLinesSize = rb.getNbLines();

		constexpr size_t linesPerDirLight = 3ull;
		size_t debugDirLightLinesCount = 0;
		auto dirLightsView = m_Registry.view<DirectionalLightComponent>();
		debugDirLightLinesCount = dirLightsView.size() * linesPerDirLight;

		m_DebugLinesToDraw.clear();
		m_DebugLinesToDraw.reserve(debugCollisionsLinesSize + m_UserDebugLines.size() + debugDirLightLinesCount);
		m_DebugTrianglesToDraw.clear();

		// Gather Debug data
		{
			// Debug point lights attenuation radii
			{
				for (auto& lightEntityID : m_PointLightsDebugRadii)
				{
					Entity entity((entt::entity)lightEntityID, this);
					if (!entity.HasComponent<PointLightComponent>())
						return;

					const auto& light = entity.GetComponent<PointLightComponent>();
					const glm::vec3& center = light.GetWorldTransform().Location;
					const float radius = light.GetRadius();
					Utils::DrawSphere(m_DebugLinesToDraw, center, glm::vec3(0, 1, 0), radius);
				}
			}

			// Debug spot lights attenuation distance
			{
				for (auto& lightEntityID : m_SpotLightsDebugRadii)
				{
					Entity entity((entt::entity)lightEntityID, this);
					if (!entity.HasComponent<SpotLightComponent>())
						return;

					const auto& light = entity.GetComponent<SpotLightComponent>();

					const glm::vec3& location = light.GetWorldTransform().Location;
					const float distance = light.GetDistance();
					const glm::vec3 center = location + light.GetForwardVector() * distance;
					const glm::quat quat = light.GetWorldTransform().Rotation.GetQuat();
					const float innerRadius = distance * glm::tan(glm::radians(light.GetInnerCutOffAngle()));
					const float outerRadius = distance * glm::tan(glm::radians(light.GetOuterCutOffAngle()));

					for (uint32_t i = 0; i < Utils::s_SphereLinesCount; ++i)
					{
						const float angle1 = (float(i) / Utils::s_SphereLinesCount) * Utils::s_2PI;
						const float angle2 = (float(i + 1) / Utils::s_SphereLinesCount) * Utils::s_2PI;
						const float cosAngle1 = glm::cos(angle1);
						const float cosAngle2 = glm::cos(angle2);
						const float sinAngle1 = glm::sin(angle1);
						const float sinAngle2 = glm::sin(angle2);

						const glm::vec3 innerStart = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
						auto& innerCircleLine = m_DebugLinesToDraw.emplace_back();
						innerCircleLine.Start.Location = innerStart;
						innerCircleLine.End.Location = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));

						auto& toInnerLine = m_DebugLinesToDraw.emplace_back();
						toInnerLine.Start.Location = location;
						toInnerLine.End.Location = innerStart;

						const glm::vec3 outerStart = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
						auto& outerCircleLine = m_DebugLinesToDraw.emplace_back();
						outerCircleLine.Start.Location = outerStart;
						outerCircleLine.End.Location = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));
						outerCircleLine.Start.Color = glm::vec3(0.75, 0.75f, 0.f);
						outerCircleLine.End.Color = glm::vec3(0.75, 0.75f, 0.f);

						auto& toOuterLine = m_DebugLinesToDraw.emplace_back();
						toOuterLine.Start.Location = location;
						toOuterLine.End.Location = outerStart;
						toOuterLine.Start.Color = glm::vec3(0.75, 0.75f, 0.f);
						toOuterLine.End.Color = glm::vec3(0.75, 0.75f, 0.f);
					}
				}
			}

			// Debug reverb boxes distance
			{
				for (auto& reverbEntityID : m_ReverbDebugBoxes)
				{
					Entity entity((entt::entity)reverbEntityID, this);
					if (!entity.HasComponent<ReverbComponent>())
						return;

					const auto& reverb = entity.GetComponent<ReverbComponent>();
					const glm::vec3& center = reverb.GetReverb()->GetPosition();
					Utils::DrawSphere(m_DebugLinesToDraw, center, glm::vec3(0, 1, 0), reverb.GetMinDistance());
					Utils::DrawSphere(m_DebugLinesToDraw, center, glm::vec3(1, 0, 0), reverb.GetMaxDistance());
				}
			}

			// Debug dir lights direction
			{
				for (auto& lightEntityID : m_DirLightsDebugDirection)
				{
					Entity entity((entt::entity)lightEntityID, this);
					if (!entity.HasComponent<DirectionalLightComponent>())
						return;

					const auto& dir = entity.GetComponent<DirectionalLightComponent>();
					const glm::vec3& location = dir.GetWorldTransform().Location;
					const glm::vec3 forward = dir.GetForwardVector();
					const glm::vec3 endLocation = location + forward * 0.2f;

					DrawArrow(location, endLocation, dir.GetUpVector());
				}
			}

			// Debug collisions
			if (debugCollisionsLinesSize)
			{
				const physx::PxDebugLine* physicsLines = rb.getLines();

				for (uint32_t i = 0; i < debugCollisionsLinesSize; ++i)
				{
					auto& line = physicsLines[i];
					RendererLine rendererLine;
					rendererLine.Start.Location = PhysXUtils::FromPhysXVector(line.pos0);
					rendererLine.End.Location = PhysXUtils::FromPhysXVector(line.pos1);
					m_DebugLinesToDraw.push_back(rendererLine);
				}
			}

			// Bones
			if (bDrawBones)
			{
				auto view = m_Registry.view<SkeletalMeshComponent>();
				for (auto entity : view)
				{
					auto& skeletal = view.get<SkeletalMeshComponent>(entity);
					if (auto& asset = skeletal.GetMeshAsset())
						Utils::DrawBones(m_DebugLinesToDraw, asset->GetMesh()->GetSkeletalMeshInfo().RootBone, skeletal.LastPose, skeletal.IsRagdollEnabled(), Math::ToTransformMatrix(skeletal.GetWorldTransform()));
				}
			}

			// AABBs
			if (true)
			{
				if (bDrawMeshAABBs)
				{
					auto view = m_Registry.view<SkeletalMeshComponent>();
					for (auto entity : view)
					{
						const auto& skeletal = view.get<SkeletalMeshComponent>(entity);
						if (const auto& asset = skeletal.GetMeshAsset())
							Utils::DrawAABB(m_DebugLinesToDraw, asset->GetMesh()->GetAABB(), skeletal.GetWorldTransform());
					}
				}
				if (bDrawMeshAABBs)
				{
					auto view = m_Registry.view<StaticMeshComponent>();
					for (auto entity : view)
					{
						const auto& staticMesh = view.get<StaticMeshComponent>(entity);
						if (const auto& asset = staticMesh.GetMeshAsset())
							Utils::DrawAABB(m_DebugLinesToDraw, asset->GetMesh()->GetAABB(), staticMesh.GetWorldTransform());
					}
				}
				if (false)
				{
					const float aspect = float(m_ViewportWidth) / m_ViewportHeight;
					auto view = m_Registry.view<CameraComponent>();
					for (auto entity : view)
					{
						const auto& camera = view.get<CameraComponent>(entity);
						Utils::DrawFrustum(m_DebugLinesToDraw, camera.Camera, camera.GetWorldTransform(), aspect);
					}
				}
				if (false)
				{
					auto view = m_Registry.view<ParticleSystemComponent>();
					for (auto entity : view)
					{
						const auto& system = view.get<ParticleSystemComponent>(entity);
						const auto& asset = system.GetAsset();
						if (!asset)
							continue;

						for (const auto& emitter : asset->GetEmitters())
						{
							Utils::DrawBox(m_DebugLinesToDraw, emitter.VisibilityAABB, system.GetWorldTransform());
						}
					}
				}
				if (bDrawNavMesh)
				{
					auto view = m_Registry.view<NavigationMeshComponent>();
					for (auto entity : view)
					{
						const auto& navigation = view.get<NavigationMeshComponent>(entity);
						const auto& settings = navigation.GetSettings();
						const auto& aabb = settings.AABB;
						Utils::DrawBox(m_DebugLinesToDraw, aabb, navigation.GetWorldTransform(), glm::vec3(1, 0, 0));

						AINavigation::DebugDraw debugDraw(m_DebugLinesToDraw, m_DebugTrianglesToDraw);
						navigation.GetNavMeshDebugDraw(&debugDraw);
					}
				}
				if (false)
				{
					if (m_CurrentNavMesh)
					{
						const glm::vec3 start = { 2.1703f, 0.f, -2.2557f };
						const glm::vec3 end = { -2.1192f, 0.f, 2.3706 };
						std::vector<glm::vec3> path = m_CurrentNavMesh->FindSmoothPath(start, end);
						if (!path.empty())
						{
							const size_t count = path.size();
							glm::vec3 startPos = path.front();
							for (size_t i = 1; i < count; i++)
							{
								RendererLine line;
								line.Start.Location = startPos;
								line.End.Location = path[i];
								DrawDebugLine(line);
								startPos = path[i];
							}
						}
					}
				}
				if (false)
				{
					auto view = m_Registry.view<BoxColliderComponent>();
					for (auto entity : view)
					{
						const auto& component = view.get<BoxColliderComponent>(entity);
						const auto& shape = component.GetShape();
						std::vector<glm::vec3> vertices;
						std::vector<uint32_t> indices;

						Transform pose = shape->GetLocalTransform();
						Transform tBody = shape->GetGlobalTransform();
						glm::mat4 t = Math::ToTransformMatrix(tBody + pose);

						shape->GetGeometry(vertices, indices);

						for (uint32_t i = 0; i < indices.size(); i += 3)
						{
							auto& line1 = m_DebugLinesToDraw.emplace_back();
							line1.Start.Location = t * glm::vec4(vertices[indices[i]], 1.f);
							line1.End.Location = t * glm::vec4(vertices[indices[i + 1]], 1.f);

							auto& line2 = m_DebugLinesToDraw.emplace_back();
							line2.Start.Location = t * glm::vec4(vertices[indices[i]], 1.f);
							line2.End.Location = t * glm::vec4(vertices[indices[i + 2]], 1.f);

							auto& line3 = m_DebugLinesToDraw.emplace_back();
							line3.Start.Location = t * glm::vec4(vertices[indices[i + 1]], 1.f);
							line3.End.Location = t * glm::vec4(vertices[indices[i + 2]], 1.f);
						}
					}
				}
				if (false)
				{
					auto view = m_Registry.view<CapsuleColliderComponent>();
					for (auto entity : view)
					{
						const auto& component = view.get<CapsuleColliderComponent>(entity);
						const auto& shape = component.GetShape();
						const glm::vec3& location = component.GetWorldTransform().Location;

						const glm::vec3& scale = shape->GetColliderScale();
						const float& radius = scale.x;
						const float& height = scale.y;

						const glm::vec3 halfExtent1 = glm::vec3(radius, 0.f, radius);
						const glm::vec3 halfExtent2 = glm::vec3(radius, height, radius);
						AABB aabb(location - halfExtent1, location + halfExtent2);

						Utils::DrawBox(m_DebugLinesToDraw, aabb, {}, glm::vec3(0, 0, 1));
					}
				}
				for (const auto& [aabb, transform] : m_UserAABBs)
				{
					Utils::DrawAABB(m_DebugLinesToDraw, aabb, transform);
				}
				for (const auto& [aabb, transform] : m_UserBoxes)
				{
					Utils::DrawBox(m_DebugLinesToDraw, aabb, transform);
				}
				m_UserAABBs.clear();
				m_UserBoxes.clear();
			}

			// Append user provided lines
			m_DebugLinesToDraw.insert(m_DebugLinesToDraw.end(), m_UserDebugLines.begin(), m_UserDebugLines.end());
			m_DebugTrianglesToDraw.insert(m_DebugTrianglesToDraw.end(), m_UserDebugTriangles.begin(), m_UserDebugTriangles.end());
			// User provided lines need to provided each frame. So clear it.
			m_UserDebugLines.clear();
			m_UserDebugTriangles.clear();
		}

		// Text components
		if (m_DirtyFlags.bTextDirty)
		{
			auto view = m_Registry.view<TextComponent>();
			m_Texts.clear();

			for (auto entity : view)
			{
				auto& text = view.get<TextComponent>(entity);
				if (text.IsVisible())
					m_Texts.push_back(&text);
			}
		}

		// Text2D components
		if (m_DirtyFlags.bText2DDirty)
		{
			auto view = m_Registry.view<Text2DComponent>();
			m_Texts2D.clear();

			for (auto entity : view)
			{
				auto& text = view.get<Text2DComponent>(entity);
				if (text.IsVisible())
					m_Texts2D.push_back(&text);
			}
		}

		// Image2D components
		if (m_DirtyFlags.bImage2DDirty)
		{
			auto view = m_Registry.view<Image2DComponent>();
			m_Images2D.clear();

			for (auto entity : view)
			{
				auto& image2D = view.get<Image2DComponent>(entity);
				if (image2D.IsVisible())
					m_Images2D.push_back(&image2D);
			}
		}

		// Particle Systems
		if (m_DirtyFlags.bRecreateParticleSystems)
		{
			m_DirtyTransformParticles.clear();
			m_SkeletalParticles.clear();

			m_SceneRenderer->RemoveAllParticleSystems();

			auto view = m_Registry.view<ParticleSystemComponent>();
			for (auto entity : view)
			{
				auto& ps = view.get<ParticleSystemComponent>(entity);
				if (ps.bAutospawn)
				{
					RegisterSkeletalParticleIfCan(ps);
					m_SceneRenderer->AddParticleSystem(ps);
				}
			}
		}
		else
		{
			if (!m_DirtyTransformParticles.empty())
			{
				static_assert(std::is_same<uint32_t, EntityIDType>::value);

				m_TempParticleSystems.clear();
				for (const auto& entityID : m_DirtyTransformParticles)
				{
					entt::entity entity = (entt::entity)entityID;
					if (m_Registry.valid(entity) && m_Registry.all_of<ParticleSystemComponent>(entity))
					{
						m_TempParticleSystems.emplace(&m_Registry.get<ParticleSystemComponent>(entity));
					}
				}

				m_SceneRenderer->UpdateParticleTransforms(m_TempParticleSystems);
				m_DirtyTransformParticles.clear();
			}
		}

		const Camera* camera = bIsPlaying ? (Camera*)&m_RuntimeCamera->Camera : (Camera*)&EditorCamera;
		if (m_DirtyFlags.bPointLightsDirty)
			m_SceneRenderer->SetPointLights(m_PointLights);
		if (m_DirtyFlags.bSpotLightsDirty)
			m_SceneRenderer->SetSpotLights(m_SpotLights);
		m_SceneRenderer->SetDirectionalLight(m_DirectionalLights.empty() ? nullptr : m_DirectionalLights[0]);
		if (m_DirtyFlags.bStaticMeshesDirty)
			m_SceneRenderer->SetMeshes(m_Meshes);
		if (m_DirtyFlags.bSkeletalMeshesDirty)
			m_SceneRenderer->SetSkeletalMeshes(m_SkeletalMeshes);
		if (m_DirtyFlags.bSpritesDirty)
			m_SceneRenderer->SetSprites(m_Sprites);
		m_SceneRenderer->SetDebugLines(m_DebugLinesToDraw);
		m_SceneRenderer->SetDebugTriangles(m_DebugTrianglesToDraw);
		m_SceneRenderer->SetBillboards(m_Billboards);
		if (m_DirtyFlags.bTextDirty)
			m_SceneRenderer->SetTexts(m_Texts);
		if (m_DirtyFlags.bText2DDirty)
			m_SceneRenderer->SetTexts2D(m_Texts2D);
		if (m_DirtyFlags.bImage2DDirty)
			m_SceneRenderer->SetImages2D(m_Images2D);
		m_SceneRenderer->SetIsRuntime(bIsPlaying);
		m_SceneRenderer->SetMeshesAnimationTransforms(std::move(m_AnimationTransforms));
		m_SceneRenderer->SetSkeletalParticleAnimationTransforms(std::move(m_SkeletalParticlesAnimationTransforms));
		m_SceneRenderer->SetGravity(m_RuntimePhysicsSettings.Gravity);
		if (m_DirtyFlags.bDecalsDirty)
			m_SceneRenderer->SetDecals(m_Decals);

		if (!m_DebugCameras.empty())
		{
			const float aspect = float(m_ViewportWidth) / m_ViewportHeight;
			const uint32_t cameraEntityID = *m_DebugCameras.begin();
			Entity entity((entt::entity)cameraEntityID, this);

			EG_CORE_ASSERT(entity.HasComponent<CameraComponent>());
			if (entity.HasComponent<CameraComponent>())
			{
				const auto& camera = entity.GetComponent<CameraComponent>();
				m_SceneRenderer->SetDebugFrustumCulling(camera.GetWorldTransform().Location, camera.GetViewMatrix(), camera.Camera, aspect);
			}
		}

		const bool bDrawEditorHelpers = !bIsPlaying && bDrawMiscellaneous;
		m_SceneRenderer->SetGridEnabled(bDrawEditorHelpers);

		// Add engine billboards if necessary
		if (bDrawEditorHelpers)
		{
			Transform transform;
			transform.Scale3D = glm::vec3(0.25f);
			for (auto& point : m_PointLights)
			{
				transform.Location = point->GetWorldTransform().Location;
				m_SceneRenderer->AddAdditionalBillboard(transform, Texture2D::PointLightIcon, (int)point->Parent.GetID());
			}
			for (auto& spot : m_SpotLights)
			{
				transform = spot->GetWorldTransform();
				transform.Scale3D = glm::vec3(0.25f);

				m_SceneRenderer->AddAdditionalBillboard(transform, Texture2D::SpotLightIcon, (int)spot->Parent.GetID());
			}
			for (const auto& dir : m_DirectionalLights)
			{
				transform = dir->GetWorldTransform();
				transform.Scale3D = glm::vec3(0.25f);
				m_SceneRenderer->AddAdditionalBillboard(transform, Texture2D::DirectionalLightIcon, (int)dir->Parent.GetID());
			}
		}

		const glm::mat4& viewMatrix = bIsPlaying ? m_RuntimeCamera->GetViewMatrix() : EditorCamera.GetViewMatrix();
		const glm::vec3& viewPos = bIsPlaying ? m_RuntimeCamera->GetWorldTransform().Location : EditorCamera.GetLocation();
		const glm::vec3& viewDir = bIsPlaying ? m_RuntimeCamera->GetForwardVector() : EditorCamera.GetForwardVector();
		{
			EG_CPU_TIMING_SCOPED("Scene. Render");
			m_SceneRenderer->Render(camera, viewMatrix, viewPos, viewDir);
		}

		m_DirtyTransformStaticMeshes.clear();
		m_DirtyTransformSkeletalMeshes.clear();
		m_DirtyTransformSprites.clear();
		m_DirtyTransformTexts.clear();
		m_DirtyFlags.SetEverythingDirty(false);
	}

	void Scene::OnRuntimeStart()
	{
		bIsPlaying = true;

		// Update Audio
		{
			auto view = m_Registry.view<AudioComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				auto& comp = e.GetComponent<AudioComponent>();
				if (comp.bAutoplay)
					comp.Play();
			}
		}
		
		// Update C# scripts
		{
			auto view = m_Registry.view<ScriptComponent>();

			// Instantiate all entities
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::InstantiateEntityClass(e);
			}

			// When all entities were instantiated,
			// call 'OnCreate'
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::OnCreateEntity(e);
			}
		}

		m_PhysicsScene->SetGravity(m_RuntimePhysicsSettings.Gravity);
		m_PhysicsScene->SetUpdateRate(m_RuntimePhysicsSettings.UpdateRate);
		m_PhysicsScene->SetDebugOnPlay(m_RuntimePhysicsSettings.bDebugOnPlay);
		m_PhysicsScene->SetDebugType(m_RuntimePhysicsSettings.DebugType);
		m_PhysicsScene->StartDebugging();
	}

	void Scene::OnRuntimeStop()
	{
		{
			auto view = m_Registry.view<NativeScriptComponent>();
			for (auto& e : view)
			{
				auto& nsc = m_Registry.get<NativeScriptComponent>(e);
				nsc.Destroy();
			}
		}
		{
			auto view = m_Registry.view<ScriptComponent>();
			for (auto& e : view)
			{
				auto& sc = m_Registry.get<ScriptComponent>(e);
				ScriptEngine::OnDestroyEntity(Entity{e, this});
			}

			// Destroy script instances
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::RemoveEntityScript(e);
			}
		}

		bIsPlaying = false;
		m_PhysicsScene->StopDebugging();
		m_PhysicsScene->Reset();
	}

	Entity Scene::CreateFromEntityAsset(const Ref<AssetEntity>& asset, bool bCopyGUID)
	{
		Entity createdEntity = CreateFromEntity(*asset->GetEntity().get(), bCopyGUID);
		createdEntity.SetName(Utils::AsString(asset->GetPath().stem()));
		if (createdEntity.HasComponent<EntityAssetComponent>())
		{
			createdEntity.GetComponent<EntityAssetComponent>().AssetGUID = asset->GetGUID();
		}
		else
		{
			createdEntity.AddComponent<EntityAssetComponent>().AssetGUID = asset->GetGUID();
		}

		return createdEntity;
	}

	void Scene::ReloadEntitiesCreatedFromAsset(const Ref<AssetEntity>& asset)
	{
		auto view = m_Registry.view<EntityAssetComponent>();
		const GUID assetID = asset->GetGUID();
		const Entity assetEntity = *asset->GetEntity().get();

		for (auto& e : view)
		{
			const auto& id = m_Registry.get<EntityAssetComponent>(e).AssetGUID;
			if (id == assetID)
			{
				Entity thisEntity = Entity(e, this);
				Transform transform = thisEntity.GetWorldTransform();
				DestroyEntity(thisEntity, true);

				Entity newEntity = CreateFromEntityAsset(asset);
				newEntity.SetWorldTransform(transform);
			}
		}
	}

	void Scene::OnEventRuntime(Event& e)
	{
		//Running Scripts
		{
			auto view = m_Registry.view<NativeScriptComponent>();

			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);
				nsc.OnEvent(Entity{ entity, this }, e);
			}
		}

		// C# scripts
		{
			// Create a single event object for all `OnEvent` invocations
			std::array params = e.GetData();
			void* eventObject = ScriptEngine::Construct(e.GetCSharpCtor(), true, params.data());

			auto view = m_Registry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::OnEventEntity(e, eventObject);
			}
		}
	}

	void Scene::OnEventEditor(Event& e)
	{
		EditorCamera.OnEvent(e);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		EditorCamera.SetViewportSize(width, height);
		m_SceneRenderer->SetViewportSize({ width, height });
	}

	void Scene::DestroyScripts()
	{
		if (bIsPlaying)
		{
			//Calling on destroy for C++ scripts
			{
				auto view = m_Registry.view<NativeScriptComponent>();
				for (auto entity : view)
				{
					auto& nsc = view.get<NativeScriptComponent>(entity);
					nsc.Destroy();
				}
			}

			//Calling on destroy for C# scripts
			if (bIsPlaying)
			{
				auto view = m_Registry.view<ScriptComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };
					ScriptEngine::OnDestroyEntity(e);
				}
			}
		}
	}

	void Scene::ClearScene()
	{
		DestroyScripts();

		m_PhysicsScene->Reset();
		m_CurrentNavMesh.reset();
		m_SpawnedSounds.clear();
		m_Registry.clear();
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();

		for (auto entityID : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entityID);
			if (cameraComponent.Primary)
			{
				return Entity{entityID, this};
			}
		}

		return Entity::Null;
	}

	void Scene::DrawArrow(const glm::vec3& start, const glm::vec3& end, const glm::vec3& up)
	{
		const glm::vec3 dir = glm::normalize(end - start);

		RendererLine line;
		line.Start.Location = start;
		line.End.Location = end;
		m_UserDebugLines.push_back(line);

		line.Start.Location = start + dir * 0.15f + up * 0.05f;
		m_UserDebugLines.push_back(line);

		line.Start.Location = start + dir * 0.15f + up * -0.05f;
		m_UserDebugLines.push_back(line);
	}

	void Scene::DrawCone(const glm::vec3& location, const glm::quat& rotation, float distance, float angleRad)
	{
		const glm::vec3 direction = Math::GetForwardVector(rotation);
		const glm::vec3 center = location + direction * distance;
		const glm::quat& quat = rotation;
		const float radius = distance * glm::tan(angleRad);

		for (uint32_t i = 0; i < Utils::s_SphereLinesCount; ++i)
		{
			const float angle1 = (float(i) / Utils::s_SphereLinesCount) * Utils::s_2PI;
			const float angle2 = (float(i + 1) / Utils::s_SphereLinesCount) * Utils::s_2PI;
			const float cosAngle1 = glm::cos(angle1);
			const float cosAngle2 = glm::cos(angle2);
			const float sinAngle1 = glm::sin(angle1);
			const float sinAngle2 = glm::sin(angle2);

			const glm::vec3 start = center + glm::rotate(quat, radius * glm::vec3(cosAngle1, sinAngle1, 0.f));
			auto& innerCircleLine = m_UserDebugLines.emplace_back();
			innerCircleLine.Start.Location = start;
			innerCircleLine.End.Location = center + glm::rotate(quat, radius * glm::vec3(cosAngle2, sinAngle2, 0.f));

			auto& toInnerLine = m_UserDebugLines.emplace_back();
			toInnerLine.Start.Location = location;
			toInnerLine.End.Location = start;
		}
	}

	void Scene::DrawFrustum(const CameraComponent& camera)
	{
		const float aspect = float(m_ViewportWidth) / m_ViewportHeight;
		Utils::DrawFrustum(m_UserDebugLines, camera.Camera, camera.GetWorldTransform(), aspect);
	}

	SceneSoundData Scene::SpawnSound2D(const Ref<AssetAudio>& audio, const SoundSettings& settings)
	{
		SceneSoundData result;
		result.Sound = Sound2D::Create(audio->GetAudio(), settings);
		m_SpawnedSounds[result.ID] = result.Sound;
		return result;
	}

	SceneSoundData Scene::SpawnSound3D(const Ref<AssetAudio>& audio, const glm::vec3& position, RollOffModel rollOff, const SoundSettings& settings)
	{
		SceneSoundData result;
		result.Sound = Sound3D::Create(audio->GetAudio(), position, rollOff, settings);
		m_SpawnedSounds[result.ID] = result.Sound;
		return result;
	}

	Ref<Sound> Scene::GetSpawnedSound(GUID id) const
	{
		auto it = m_SpawnedSounds.find(id);
		return it != m_SpawnedSounds.end() ? it->second : nullptr;
	}

	Entity Scene::GetEntityByGUID(const GUID& guid) const
	{
		auto it = m_AliveEntities.find(guid);
		return it != m_AliveEntities.end() ? it->second : Entity::Null;
	}

	const Ref<PhysicsActor>& Scene::GetPhysicsActor(const Entity& entity) const
	{
		if (m_PhysicsScene)
			return m_PhysicsScene->GetPhysicsActor(entity);

		return s_NullPhysicsActor;
	}

	Ref<PhysicsActor>& Scene::GetPhysicsActor(const Entity& entity)
	{
		if (m_PhysicsScene)
			return m_PhysicsScene->GetPhysicsActor(entity);

		return s_NullPhysicsActor;
	}

	void Scene::SetGUID(GUID guid)
	{
		ScriptEngine::RemoveOnAppAssemblyReloadedCallback(m_GUID);

		m_GUID = guid;
		SetupOnAppAssemblyReloadedCallback();
	}

	void Scene::SetGravity(const glm::vec3& gravity)
	{
		m_RuntimePhysicsSettings.Gravity = gravity;
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetGravity(m_RuntimePhysicsSettings.Gravity);
	}

	void Scene::SetPhysicsUpdateRate(uint32_t updateRate)
	{
		m_RuntimePhysicsSettings.UpdateRate = glm::clamp(updateRate, PhysicsSettings::s_MinUpdateRate, PhysicsSettings::s_MaxUpdateRate);
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetUpdateRate(m_RuntimePhysicsSettings.UpdateRate);
	}

	void Scene::SetPhysicsDebugOnPlay(bool bEnable)
	{
		m_RuntimePhysicsSettings.bDebugOnPlay = bEnable;
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetDebugOnPlay(bEnable);
	}

	void Scene::SetPhysicsDebugType(DebugType type)
	{
		m_RuntimePhysicsSettings.DebugType = type;
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetDebugType(type);
	}

	void Scene::InvalidateCollisionGroups(uint32_t validMasks)
	{
		Utils::InvalidateCollisionGroups<BoxColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<SphereColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<CapsuleColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<MeshColliderComponent>(m_Registry, validMasks);
	}

	CameraComponent* Scene::GetRuntimeCamera()
	{
		return m_RuntimeCamera;
	}

	void Scene::AddParticleSystem(const ParticleSystemComponent& system)
	{
		RegisterSkeletalParticleIfCan(system);
		m_SceneRenderer->AddParticleSystem(system);
	}

	void Scene::RemoveParticleSystem(const ParticleSystemComponent& system)
	{
		m_SkeletalParticles.erase(system.Parent.GetID());
		m_SceneRenderer->RemoveParticleSystem(system);
	}

	void Scene::UpdateParticleSystem(const ParticleSystemComponent& system)
	{
		m_DirtyTransformParticles.erase(system.Parent.GetID()); // No need to update transform separately
		m_SkeletalParticles.erase(system.Parent.GetID());
		RegisterSkeletalParticleIfCan(system);
		m_SceneRenderer->UpdateParticleSystem(system);
	}

	void Scene::OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& sm = entity.GetComponent<StaticMeshComponent>().GetMeshAsset();
		if (sm && sm->GetMesh()->IsValid())
		{
			m_DirtyFlags.bStaticMeshesDirty = true;
			m_DirtyFlags.bStaticMeshTransformsDirty = true;
		}
	}

	void Scene::OnSkeletalMeshComponentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& sm = entity.GetComponent<SkeletalMeshComponent>().GetMeshAsset();
		if (sm && sm->GetMesh()->IsValid())
		{
			m_DirtyFlags.bSkeletalMeshesDirty = true;
			m_DirtyFlags.bSkeletalMeshTransformsDirty = true;
		}
	}

	void Scene::OnSpriteComponentAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bSpritesDirty = true;
		m_DirtyFlags.bSpriteTransformsDirty = true;
	}

	void Scene::OnDecalComponentAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bDecalsDirty = true;
		m_DirtyFlags.bDecalTransformsDirty = true;
	}

	void Scene::OnPointLightAdded(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bPointLightsDirty = true;
	}

	void Scene::OnPointLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& light = entity.GetComponent<PointLightComponent>();
		if (light.DoesAffectWorld())
		{
			m_DirtyFlags.bPointLightsDirty = true;
		}
		m_PointLightsDebugRadii.erase(entity.GetID());
	}

	void Scene::OnSpotLightAdded(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bSpotLightsDirty = true;
	}

	void Scene::OnSpotLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& light = entity.GetComponent<SpotLightComponent>();
		if (light.DoesAffectWorld())
		{
			m_DirtyFlags.bSpotLightsDirty = true;
		}
		m_SpotLightsDebugRadii.erase(entity.GetID());
	}

	void Scene::OnTextAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bTextDirty = true;
		m_DirtyFlags.bTextTransformsDirty = true;
	}

	void Scene::OnText2DAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bText2DDirty = true;
	}

	void Scene::OnImage2DAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bImage2DDirty = true;
	}

	void Scene::OnParticleSystemAdded(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& comp = entity.GetComponent<ParticleSystemComponent>();
		if (comp.bAutospawn)
			comp.Spawn();
	}

	void Scene::OnParticleSystemRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<ParticleSystemComponent>().Destroy();
	}

	void Scene::OnNavMeshRemoved(entt::registry& r, entt::entity e)
	{
		if (!m_CurrentNavMesh)
			return;

		Entity entity(e, this);
		const auto& navMesh = entity.GetComponent<NavigationMeshComponent>().GetNavMesh();
		if (m_CurrentNavMesh == navMesh)
		{
			BuildNavMesh(nullptr);
		}
	}

	void Scene::OnBoxColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<BoxColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnSphereColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<SphereColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnCapsuleColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<CapsuleColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnCrowdAgentAdded(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
		component.CreateAgent(entity.GetWorldLocation());
	}

	void Scene::OnCrowdAgentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
		component.RemoveAgent();
	}

	void Scene::OnCameraRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		m_DebugCameras.erase(entity.GetID());
	}

	void Scene::OnReverbRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		m_ReverbDebugBoxes.erase(entity.GetID());
	}

	void Scene::OnDirectionalLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		m_DirLightsDebugDirection.erase(entity.GetID());
		auto& light = entity.GetComponent<DirectionalLightComponent>();
		if (light.DoesAffectWorld())
		{
			m_DirtyFlags.bDirLightsDirty = true;
		}
	}

	void Scene::ConnectSignals()
	{
		m_Registry.on_destroy<StaticMeshComponent>().connect<&Scene::OnStaticMeshComponentRemoved>(*this);
		m_Registry.on_destroy<SkeletalMeshComponent>().connect<&Scene::OnSkeletalMeshComponentRemoved>(*this);
		m_Registry.on_construct<SpriteComponent>().connect<&Scene::OnSpriteComponentAddedRemoved>(*this);
		m_Registry.on_destroy<SpriteComponent>().connect<&Scene::OnSpriteComponentAddedRemoved>(*this);
		m_Registry.on_construct<DecalComponent>().connect<&Scene::OnDecalComponentAddedRemoved>(*this);
		m_Registry.on_destroy<DecalComponent>().connect<&Scene::OnDecalComponentAddedRemoved>(*this);
		m_Registry.on_construct<PointLightComponent>().connect<&Scene::OnPointLightAdded>(*this);
		m_Registry.on_destroy<PointLightComponent>().connect<&Scene::OnPointLightRemoved>(*this);
		m_Registry.on_construct<SpotLightComponent>().connect<&Scene::OnSpotLightAdded>(*this);
		m_Registry.on_destroy<SpotLightComponent>().connect<&Scene::OnSpotLightRemoved>(*this);
		m_Registry.on_construct<TextComponent>().connect<&Scene::OnTextAddedRemoved>(*this);
		m_Registry.on_destroy<TextComponent>().connect<&Scene::OnTextAddedRemoved>(*this);
		m_Registry.on_construct<Text2DComponent>().connect<&Scene::OnText2DAddedRemoved>(*this);
		m_Registry.on_destroy<Text2DComponent>().connect<&Scene::OnText2DAddedRemoved>(*this);
		m_Registry.on_construct<Image2DComponent>().connect<&Scene::OnImage2DAddedRemoved>(*this);
		m_Registry.on_destroy<Image2DComponent>().connect<&Scene::OnImage2DAddedRemoved>(*this);
		m_Registry.on_construct<ParticleSystemComponent>().connect<&Scene::OnParticleSystemAdded>(*this);
		m_Registry.on_destroy<ParticleSystemComponent>().connect<&Scene::OnParticleSystemRemoved>(*this);
		m_Registry.on_destroy<NavigationMeshComponent>().connect<&Scene::OnNavMeshRemoved>(*this);
		m_Registry.on_destroy<BoxColliderComponent>().connect<&Scene::OnBoxColliderRemoved>(*this);
		m_Registry.on_destroy<SphereColliderComponent>().connect<&Scene::OnSphereColliderRemoved>(*this);
		m_Registry.on_destroy<CapsuleColliderComponent>().connect<&Scene::OnCapsuleColliderRemoved>(*this);
		m_Registry.on_construct<NavigationCrowdAgentComponent>().connect<&Scene::OnCrowdAgentAdded>(*this);
		m_Registry.on_destroy<NavigationCrowdAgentComponent>().connect<&Scene::OnCrowdAgentRemoved>(*this);
		m_Registry.on_destroy<CameraComponent>().connect<&Scene::OnCameraRemoved>(*this);
		m_Registry.on_destroy<ReverbComponent>().connect<&Scene::OnReverbRemoved>(*this);
		m_Registry.on_destroy<DirectionalLightComponent>().connect<&Scene::OnDirectionalLightRemoved>(*this);
	}

	void Scene::RegisterSkeletalParticleIfCan(const ParticleSystemComponent& system)
	{
		const auto& asset = system.GetAsset();
		if (!asset)
			return;

		const auto& emitters = asset->GetEmitters();
		for (const auto& emitter : emitters)
		{
			if (emitter.IsSkeletalMeshUsed())
			{
				m_SkeletalParticles.emplace(system.Parent.GetID());
				break;
			}
		}
	}

	void Scene::CopyComponents(Entity source, Entity dest)
	{
		EntityCopyComponent<EntityAssetComponent>(source, dest);
		EntityCopyComponent<TagComponent>(source, dest);
		EntityCopyComponent<NativeScriptComponent>(source, dest);
		EntityCopyComponent<ScriptComponent>(source, dest);
		EntityCopyComponent<PointLightComponent>(source, dest);
		EntityCopyComponent<DirectionalLightComponent>(source, dest);
		EntityCopyComponent<SpotLightComponent>(source, dest);
		EntityCopyComponent<SpriteComponent>(source, dest);
		EntityCopyComponent<StaticMeshComponent>(source, dest);
		EntityCopyComponent<SkeletalMeshComponent>(source, dest);
		EntityCopyComponent<BillboardComponent>(source, dest);
		EntityCopyComponent<CameraComponent>(source, dest);
		EntityCopyComponent<RigidBodyComponent>(source, dest);
		EntityCopyComponent<BoxColliderComponent>(source, dest);
		EntityCopyComponent<SphereColliderComponent>(source, dest);
		EntityCopyComponent<CapsuleColliderComponent>(source, dest);
		EntityCopyComponent<MeshColliderComponent>(source, dest);
		EntityCopyComponent<AudioComponent>(source, dest);
		EntityCopyComponent<ReverbComponent>(source, dest);
		EntityCopyComponent<TextComponent>(source, dest);
		EntityCopyComponent<Text2DComponent>(source, dest);
		EntityCopyComponent<Image2DComponent>(source, dest);
		EntityCopyComponent<ParticleSystemComponent>(source, dest);
		EntityCopyComponent<DecalComponent>(source, dest);
		EntityCopyComponent<NavigationCrowdAgentComponent>(source, dest);
		EntityCopyComponent<NavigationMeshComponent>(source, dest);

		if (bIsPlaying)
		{
			if (dest.HasComponent<AudioComponent>())
			{
				auto& comp = dest.GetComponent<AudioComponent>();
				if (comp.bAutoplay)
					comp.Play();
			}

			if (dest.HasComponent<ScriptComponent>())
			{
				if (!dest.GetComponent<ScriptComponent>().ModuleName.empty() && ScriptEngine::InstantiateEntityClass(dest))
				{
					ScriptEngine::OnCreateEntity(dest);
				}
			}
		}
	}
}
