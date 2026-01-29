#include "egpch.h"
#include "PhysicsMaterialAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Physics/PhysicsMaterial.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	constexpr static Transform s_Sphere1Transform(glm::vec3(-3.f, 3.f, 0.f));
	constexpr static Transform s_Sphere2Transform(glm::vec3(+3.f, 3.f, 0.f));
	constexpr static glm::vec3 s_PlaneScale = glm::vec3(3.f, 0.05f, 10.f);

	PhysicsMaterialAssetEditor::PhysicsMaterialAssetEditor(const Ref<AssetPhysicsMaterial>& asset)
		: AssetEditor(true, true), m_Asset(asset)
	{
		SetSimulationEnabled(true);

		const auto& sphere = AssetManager::GetPreviewSphere();
		const auto& cube = AssetManager::GetPreviewCube();

		const auto& scene = GetCurrentScene();
		scene->bDrawMiscellaneous = false;

		// Plane 1
		{
			m_Plane1 = scene->CreateEntity("PhysicsMaterialAssetEditor_Plane1");
			m_Plane1.AddComponent<StaticMeshComponent>().SetMeshAsset(cube);
			m_Plane1.AddComponent<BoxColliderComponent>();

			auto tr = s_Sphere1Transform;
			tr.Location -= glm::vec3(0.f, 3.f, 0.f); // Put under the sphere
			tr.Rotation = glm::quat(0.9672f, -0.2540f, 0.f, 0.f);
			tr.Scale3D = s_PlaneScale;
			m_Plane1.SetWorldTransform(tr);
		}

		// Plane 2
		{
			m_Plane2 = scene->CreateEntity("PhysicsMaterialAssetEditor_Plane2");
			m_Plane2.AddComponent<StaticMeshComponent>().SetMeshAsset(cube);
			m_Plane2.AddComponent<BoxColliderComponent>();

			auto tr = s_Sphere2Transform;
			tr.Location -= glm::vec3(0.f, 3.f, 0.f); // Put under the sphere
			tr.Scale3D = s_PlaneScale;
			m_Plane2.SetWorldTransform(tr);
		}

		// Sphere 1
		{
			m_Sphere1 = scene->CreateEntity("PhysicsMaterialAssetEditor_Sphere1");

			auto& rigidBody = m_Sphere1.AddComponent<RigidBodyComponent>(PhysicsBodyType::Dynamic);
			rigidBody.SetEnableGravity(true);

			m_Sphere1.AddComponent<SphereColliderComponent>().SetPhysicsMaterialAsset(asset);
			m_Sphere1.AddComponent<StaticMeshComponent>().SetMeshAsset(sphere);
			m_Sphere1.SetWorldTransform(s_Sphere1Transform);
		}

		// Sphere 2
		{
			m_Sphere2 = scene->CreateEntity("PhysicsMaterialAssetEditor_Sphere2");

			auto& rigidBody = m_Sphere2.AddComponent<RigidBodyComponent>(PhysicsBodyType::Dynamic);
			rigidBody.SetEnableGravity(true);

			m_Sphere2.AddComponent<SphereColliderComponent>().SetPhysicsMaterialAsset(asset);
			m_Sphere2.AddComponent<StaticMeshComponent>().SetMeshAsset(sphere);
			m_Sphere2.SetWorldTransform(s_Sphere2Transform);
		}

		// Sun
		{
			Entity entity = scene->CreateEntity("PhysicsMaterialAssetEditor_Sun");
			entity.SetWorldRotation(glm::quat(0.707f, -0.707f, 0.f, 0.f));
			auto& sun = entity.AddComponent<DirectionalLightComponent>();
			sun.SetLightColor(glm::vec3(20.5f));
		}

		auto& camera = scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(-4.f, 0.f, 0.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		constexpr AABB aabb(s_Sphere1Transform.Location, s_Sphere2Transform.Location);

		constexpr glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 2.5f); // Move back
		camera.LookAt(center);

		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void PhysicsMaterialAssetEditor::OnImGuiRender(bool* pOpen)
	{
		static const char* s_StaticFrictionHelpMsg = "Static friction defines the amount of friction that is applied between surfaces that are not moving lateral to each-other";
		static const char* s_DynamicFrictionHelpMsg = "Dynamic friction defines the amount of friction applied between surfaces that are moving relative to each-other";

		auto& material = m_Asset->GetMaterial();

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_WindowName.c_str(), pOpen);
		UI::BeginPropertyGrid("PhysicsMaterialDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Physics Material");

		bool bPhysicsMaterialChanged = false;
		float staticFriction = material->GetStaticFriction();
		float dynamicFriction = material->GetDynamicFriction();
		float bounciness = material->GetBounciness();

		if (UI::PropertyDrag("Static Friction", staticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg))
		{
			material->SetStaticFriction(staticFriction);
			bPhysicsMaterialChanged = true;
		}
		if (UI::PropertyDrag("Dynamic Friction", dynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg))
		{
			material->SetDynamicFriction(dynamicFriction);
			bPhysicsMaterialChanged = true;
		}
		if (UI::PropertyDrag("Bounciness", bounciness, 0.1f))
		{
			material->SetBounciness(bounciness);
			bPhysicsMaterialChanged = true;
		}

		UI::TextWithSeparator("Simulation Settings");

		if (UI::Button("Simulation", "Reset"))
		{
			ResetScene();
		}
		UI::EndPropertyGrid();

		// Plane 1 rotation
		{
			glm::quat quat = m_Plane1.GetWorldRotation().GetQuat();

			if (UI::DrawQuatControl("1st Plane Rotation (Quat)", quat, glm::quat{ 1, 0, 0, 0 }, 140.f))
			{
				m_Plane1.SetWorldRotation(quat);
				WakeUpActors();
			}
		}
		// Plane 2 rotation
		{
			glm::quat quat = m_Plane2.GetWorldRotation().GetQuat();

			if (UI::DrawQuatControl("2nd Plane Rotation (Quat)", quat, glm::quat{ 1, 0, 0, 0 }, 140.f))
			{
				m_Plane2.SetWorldRotation(glm::quat(quat.w, quat.x, quat.y, quat.z));
				WakeUpActors();
			}
		}

		if (bPhysicsMaterialChanged)
		{
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		DrawViewport(false, m_WindowName);
	}
	
	void PhysicsMaterialAssetEditor::ResetScene()
	{
		m_Sphere1.SetWorldTransform(s_Sphere1Transform);
		m_Sphere1.SetLinearVelocity(glm::vec3(0));
		m_Sphere1.SetAngularVelocity(glm::vec3(0));

		m_Sphere2.SetWorldTransform(s_Sphere2Transform);
		m_Sphere2.SetLinearVelocity(glm::vec3(0));
		m_Sphere2.SetAngularVelocity(glm::vec3(0));
	}
	
	void PhysicsMaterialAssetEditor::WakeUpActors()
	{
		m_Sphere1.GetComponent<RigidBodyComponent>().WakeUp();
		m_Sphere2.GetComponent<RigidBodyComponent>().WakeUp();
	}
}
