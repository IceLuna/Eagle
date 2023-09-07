#pragma once

#include "SceneComponent.h"

#include "Eagle/Core/ScriptableEntity.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Camera/SceneCamera.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/VidWrappers/SubTexture2D.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Script/PublicField.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsMaterial.h"
#include "Eagle/Audio/Sound3D.h"
#include "Eagle/Audio/Reverb3D.h"
#include "Eagle/UI/Font.h"

// If new component class is created, other changes are required:
// 1) Add new line into Scene's copy constructor;
// 2) Add new line into Scene::CreateFromEntity function;
// 3) Make it serializable;
// 4) Add it to SceneHierarchyPanel to draw UI (optional)
// 5) Add to ScriptEngineRegistry (optional)

namespace Eagle
{
	class BoxColliderShape;
	class SphereColliderShape;
	class CapsuleColliderShape;
	class MeshShape;
	class PhysicsActor;

	class IDComponent : public Component
	{
	public:
		IDComponent() = default;
		IDComponent(const GUID& other) : Component(), ID(other) {}
		COMPONENT_DEFAULTS(IDComponent);

		GUID ID;
	};

	class OwnershipComponent : public Component
	{
	public:
		OwnershipComponent() = default;

		OwnershipComponent& operator= (const OwnershipComponent& other)
		{
			if (this == &other)
				return *this;

			Component::operator=(other);

			const Scene* srcScene = other.Parent.GetScene();
			const Scene* destScene = Parent.GetScene();

			EG_CORE_ASSERT(srcScene, "Empty src Scene");
			EG_CORE_ASSERT(destScene, "Empty dest Scene");
			EG_CORE_ASSERT(srcScene != destScene, "Scene's are equal");
			
			Children.clear();
			for (auto& srcChild : other.Children)
			{
				Entity destChild = destScene->GetEntityByGUID(srcChild.GetComponent<IDComponent>().ID);
				destChild.SetParent(Parent);
			}

			return *this;
		}

		OwnershipComponent(OwnershipComponent&&) = default;
		OwnershipComponent& operator= (OwnershipComponent&&) = default;

		Entity EntityParent = Entity::Null;
		std::vector<Entity> Children;
	};

	class EntitySceneNameComponent : public Component
	{
	public:
		EntitySceneNameComponent() = default;
		COMPONENT_DEFAULTS(EntitySceneNameComponent);
		EntitySceneNameComponent(const std::string& name) : Component(name) {}
	};

	class TransformComponent : public Component
	{
	public:
		TransformComponent() = default;
		COMPONENT_DEFAULTS(TransformComponent);
		Transform WorldTransform;
		Transform RelativeTransform;
	};

	class LightComponent : public SceneComponent
	{
	public:
		LightComponent() = default;
		LightComponent(const glm::vec3& lightColor) : m_LightColor(lightColor) {}
		COMPONENT_DEFAULTS(LightComponent);

		const glm::vec3& GetLightColor() const { return m_LightColor; }
		bool DoesAffectWorld() const { return m_bAffectsWorld; }
		float GetIntensity() const { return m_Intensity; }
		float GetVolumetricFogIntensity() const { return m_VolumetricFogIntensity; }
		bool DoesCastShadows() const { return m_bCastsShadows; }
		bool IsVolumetricLight() const { return m_bVolumetricLight; }

		virtual void SetLightColor(const glm::vec3& lightColor)
		{
			m_LightColor = lightColor;
		}

		virtual void SetAffectsWorld(bool bAffects)
		{
			m_bAffectsWorld = bAffects;
		}

		virtual void SetIntensity(float intensity)
		{
			m_Intensity = intensity;
		}

		virtual void SetVolumetricFogIntensity(float intensity)
		{
			m_VolumetricFogIntensity = glm::max(0.f, intensity);
		}

		virtual void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
		}

		virtual void SetIsVolumetricLight(bool bVolumetric)
		{
			m_bVolumetricLight = bVolumetric;
		}
		
	protected:
		glm::vec3 m_LightColor = glm::vec3(1.f);
		float m_Intensity = 1.f;
		float m_VolumetricFogIntensity = 1.f;
		bool m_bAffectsWorld = true;
		bool m_bCastsShadows = true;
		bool m_bVolumetricLight = false;
	};

	class PointLightComponent : public LightComponent
	{
	public:
		PointLightComponent() = default;
		PointLightComponent(const PointLightComponent&) = delete;
		PointLightComponent(PointLightComponent&& other) = default;
		PointLightComponent& operator=(PointLightComponent&& other) = default;

		PointLightComponent& operator=(const PointLightComponent& other)
		{
			if (this == &other)
				return *this;

			LightComponent::operator=(other);
			m_Radius = other.m_Radius;
			m_VisualizeRadiusEnabled = other.m_VisualizeRadiusEnabled;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			LightComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			LightComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnTransformChanged);
		}

		virtual void SetLightColor(const glm::vec3& lightColor) override
		{
			m_LightColor = lightColor;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetAffectsWorld(bool bAffects) override
		{
			m_bAffectsWorld = bAffects;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetIntensity(float intensity) override
		{
			m_Intensity = intensity;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetVolumetricFogIntensity(float intensity) override
		{
			m_VolumetricFogIntensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		void SetRadius(float radius)
		{
			m_Radius = glm::max(0.f, radius);
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		float GetRadius() const { return m_Radius; }

		void SetVisualizeRadiusEnabled(bool bEnabled)
		{
			if (m_VisualizeRadiusEnabled != bEnabled)
			{
				m_VisualizeRadiusEnabled = bEnabled;
				Parent.SignalComponentChanged<PointLightComponent>(Notification::OnDebugStateChanged);
			}
		}
		bool VisualizeRadiusEnabled() const { return m_VisualizeRadiusEnabled; }

		virtual void SetCastsShadows(bool bCasts) override
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetIsVolumetricLight(bool bVolumetric) override
		{
			m_bVolumetricLight = bVolumetric;
			Parent.SignalComponentChanged<PointLightComponent>(Notification::OnStateChanged);
		}

	private:
		float m_Radius = 1.f;
		bool m_VisualizeRadiusEnabled = false;
	};

	class DirectionalLightComponent : public LightComponent
	{
	public:
		DirectionalLightComponent() = default;
		DirectionalLightComponent(const glm::vec3& lightColor)
			: LightComponent(lightColor) {}

		COMPONENT_DEFAULTS(DirectionalLightComponent);

		glm::vec3 Ambient = glm::vec3(0.f);
		bool bVisualizeDirection = false;
	};

	class SpotLightComponent : public LightComponent
	{
	public:
		SpotLightComponent() = default;

		SpotLightComponent(const SpotLightComponent&) = delete;
		SpotLightComponent(SpotLightComponent&& other) = default;
		SpotLightComponent& operator=(SpotLightComponent&& other) = default;

		SpotLightComponent& operator=(const SpotLightComponent& other)
		{
			if (this == &other)
				return *this;

			LightComponent::operator=(other);
			m_InnerCutOffAngle = other.m_InnerCutOffAngle;
			m_OuterCutOffAngle = other.m_OuterCutOffAngle;
			m_Distance = other.m_Distance;
			m_VisualizeDistanceEnabled = other.m_VisualizeDistanceEnabled;

			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			LightComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			LightComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnTransformChanged);
		}

		void SetLightColor(const glm::vec3& lightColor) override
		{
			m_LightColor = lightColor;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetAffectsWorld(bool bAffects) override
		{
			m_bAffectsWorld = bAffects;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetIntensity(float intensity) override
		{
			m_Intensity = intensity;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetVolumetricFogIntensity(float intensity) override
		{
			m_VolumetricFogIntensity = glm::max(0.f, intensity);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		float GetInnerCutOffAngle() const { return m_InnerCutOffAngle; }
		void SetInnerCutOffAngle(float angle)
		{
			angle = glm::clamp(angle, 1.f, 80.f);

			m_InnerCutOffAngle = std::min(m_OuterCutOffAngle, angle);
			m_OuterCutOffAngle = std::max(m_OuterCutOffAngle, angle);

			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		float GetOuterCutOffAngle() const { return m_OuterCutOffAngle; }
		void SetOuterCutOffAngle(float angle)
		{
			angle = glm::clamp(angle, 1.f, 80.f);

			m_OuterCutOffAngle = std::max(m_InnerCutOffAngle, angle);
			m_InnerCutOffAngle = std::min(m_InnerCutOffAngle, angle);

			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		void SetDistance(float distance)
		{
			m_Distance = glm::max(0.f, distance);
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		float GetDistance() const { return m_Distance; }

		void SetVisualizeDistanceEnabled(bool bEnabled)
		{
			if (m_VisualizeDistanceEnabled != bEnabled)
			{
				m_VisualizeDistanceEnabled = bEnabled;
				Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnDebugStateChanged);
			}
		}
		bool VisualizeDistanceEnabled() const { return m_VisualizeDistanceEnabled; }

		virtual void SetCastsShadows(bool bCasts) override
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

		virtual void SetIsVolumetricLight(bool bVolumetric) override
		{
			m_bVolumetricLight = bVolumetric;
			Parent.SignalComponentChanged<SpotLightComponent>(Notification::OnStateChanged);
		}

	private:
		float m_InnerCutOffAngle = 25.f;
		float m_OuterCutOffAngle = 45.f;
		float m_Distance = 1.f;
		bool m_VisualizeDistanceEnabled = false;
	};

	class SpriteComponent : public SceneComponent
	{
	public:
		SpriteComponent() : m_Material(Material::Create()) { }
		SpriteComponent(const SpriteComponent&) = delete;
		SpriteComponent(SpriteComponent&&) noexcept = default;
		SpriteComponent& operator=(SpriteComponent&&) noexcept = default;

		SpriteComponent& operator=(const SpriteComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);

			m_Material = Material::Create(other.m_Material);
			m_SubTexture = other.m_SubTexture;
			SubTextureCoords = other.SubTextureCoords;
			SpriteSize = other.SpriteSize;
			SpriteSizeCoef = other.SpriteSizeCoef;
			bSubTexture = other.bSubTexture;
			m_bCastsShadows = other.m_bCastsShadows;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);

			return *this;
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnTransformChanged);
		}

		void SetIsSubTexture(bool value)
		{
			bSubTexture = value;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}
		bool IsSubTexture() const { return bSubTexture; }

		void SetSubTexture(const Ref<SubTexture2D>& subtexture)
		{
			m_SubTexture = subtexture;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}
		const Ref<SubTexture2D>& GetSubTexture() const { return m_SubTexture; }

		const Ref<Material>& GetMaterial() const { return m_Material; }
		void SetMaterial(const Ref<Material>& material)
		{
			m_Material = material;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnMaterialChanged);
		}

		void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<SpriteComponent>(Notification::OnStateChanged);
		}
		bool DoesCastShadows() const { return m_bCastsShadows; }

	public:
		glm::vec2 SubTextureCoords = {0, 0};
		glm::vec2 SpriteSize = {64, 64};
		glm::vec2 SpriteSizeCoef = {1, 1};

	private:
		Ref<Material> m_Material;
		Ref<SubTexture2D> m_SubTexture;
		bool bSubTexture = false;
		bool m_bCastsShadows = true;
	};

	class StaticMeshComponent : public SceneComponent
	{
	public:
		StaticMeshComponent() = default;
		StaticMeshComponent(const StaticMeshComponent&) = delete;
		StaticMeshComponent(StaticMeshComponent&& other) = default;
		StaticMeshComponent& operator=(StaticMeshComponent&& other) = default;

		StaticMeshComponent& operator=(const StaticMeshComponent& other)
		{
			if (this == &other)
				return *this;

			SceneComponent::operator=(other);

			const bool bHadValidMesh = m_StaticMesh && m_StaticMesh->IsValid();

			if (other.m_StaticMesh)
				m_StaticMesh = StaticMesh::Create(other.m_StaticMesh);
			else
				m_StaticMesh.reset();

			m_Material = Material::Create(other.m_Material);
			m_bCastsShadows = other.m_bCastsShadows;

			const bool bIsValidMesh = m_StaticMesh && m_StaticMesh->IsValid();

			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
			return *this;
		}

		const Ref<Eagle::StaticMesh>& GetStaticMesh() const { return m_StaticMesh; }
		void SetStaticMesh(const Ref<Eagle::StaticMesh>& mesh)
		{
			m_StaticMesh = mesh;
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnTransformChanged);
		}

		void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnStateChanged);
		}
		bool DoesCastShadows() const { return m_bCastsShadows; }

		const Ref<Material>& GetMaterial() const { return m_Material; }
		void SetMaterial(const Ref<Material>& material)
		{
			m_Material = material;
			Parent.SignalComponentChanged<StaticMeshComponent>(Notification::OnMaterialChanged);
		}

	private:
		Ref<Eagle::StaticMesh> m_StaticMesh;
		Ref<Material> m_Material = Material::Create();
		bool m_bCastsShadows = true;
	};

	class BillboardComponent : public SceneComponent
	{
	public:
		BillboardComponent() = default;
		COMPONENT_DEFAULTS(BillboardComponent);

		Ref<Texture2D> Texture;
	};

	class TextComponent : public SceneComponent
	{
	public:
		TextComponent() = default;
		COMPONENT_DEFAULTS(TextComponent);

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnTransformChanged);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnTransformChanged);
		}

		const Ref<Font>& GetFont() const { return m_Font; }
		const std::string& GetText() const { return m_Text; }
		const glm::vec3& GetColor() const { return m_Color; }
		float GetLineSpacing() const { return m_LineSpacing; }
		float GetKerning() const { return m_Kerning; }
		float GetMaxWidth() const { return m_MaxWidth; }
		bool IsLit() const { return m_bLit; }
		const glm::vec3& GetAlbedoColor() const { return m_Albedo; }
		const glm::vec3& GetEmissiveColor() const { return m_Emissive; }
		float GetMetallness() const { return m_Metallness; }
		float GetRoughness() const { return m_Roughness; }
		float GetAO() const { return m_AO; }
		float GetOpacity() const { return m_Opacity; }
		float GetOpacityMask() const { return m_OpacityMask; }
		Material::BlendMode GetBlendMode() const { return m_BlendMode; }

		void SetFont(const Ref<Font>& font)
		{
			m_Font = font;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetText(const std::string& text)
		{
			m_Text = text;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetColor(const glm::vec3& color)
		{
			m_Color = color;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetLineSpacing(float value)
		{
			m_LineSpacing = glm::max(value, 0.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetKerning(float value)
		{
			m_Kerning = glm::max(value, 0.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetMaxWidth(float value)
		{
			m_MaxWidth = glm::max(value, 0.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetIsLit(bool bLit)
		{
			m_bLit = bLit;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetAlbedoColor(const glm::vec3& albedo)
		{
			m_Albedo = albedo;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetEmissiveColor(const glm::vec3& emissive)
		{
			m_Emissive = emissive;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetMetallness(float value)
		{
			m_Metallness = glm::clamp(value, 0.f, 1.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetRoughness(float value)
		{
			m_Roughness = glm::clamp(value, 0.f, 1.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetAO(float value)
		{
			m_AO = glm::clamp(value, 0.f, 1.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetOpacity(float value)
		{
			m_Opacity = glm::clamp(value, 0.f, 1.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetOpacityMask(float value)
		{
			m_OpacityMask = glm::clamp(value, 0.f, 1.f);
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetBlendMode(Material::BlendMode blendMode)
		{
			m_BlendMode = blendMode;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}

		void SetCastsShadows(bool bCasts)
		{
			m_bCastsShadows = bCasts;
			Parent.SignalComponentChanged<TextComponent>(Notification::OnStateChanged);
		}
		bool DoesCastShadows() const { return m_bCastsShadows; }

	private:
		std::string m_Text = "Hello, World!";
		Ref<Font> m_Font;

		glm::vec3 m_Color = glm::vec3(1.f); // Used if bLit == false
		float m_LineSpacing = 0.0f;

		glm::vec3 m_Albedo = glm::vec3(1.f); // Used if bLit == true
		float m_Kerning = 0.0f;
		glm::vec3 m_Emissive = glm::vec3(0.f); // Used if bLit == true
		float m_MaxWidth = 10.0f;

		// Used if bLit == true
		Material::BlendMode m_BlendMode = Material::BlendMode::Opaque;
		float m_Metallness = 0.f;
		float m_Roughness = 0.5f;
		float m_AO = 1.f;
		float m_Opacity = 0.5f;
		float m_OpacityMask = 1.f;

		bool m_bLit = false;
		bool m_bCastsShadows = false;
	};

	class Text2DComponent : public Component
	{
	public:
		void SetFont(const Ref<Font>& font)
		{
			m_Font = font;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetText(const std::string& text)
		{
			m_Text = text;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetColor(const glm::vec3& color)
		{
			m_Color = color;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetLineSpacing(float value)
		{
			m_LineSpacing = glm::max(value, 0.f);
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetKerning(float value)
		{
			m_Kerning = glm::max(value, 0.f);
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetMaxWidth(float value)
		{
			m_MaxWidth = glm::max(value, 0.f);
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetPosition(glm::vec2 pos)
		{
			m_Pos = pos;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetScale(glm::vec2 scale)
		{
			m_Scale = scale;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetRotation(float rotationZ)
		{
			m_Rotation = rotationZ;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetOpacity(float opacity)
		{
			m_Opacity = glm::clamp(opacity, 0.f, 1.f);
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		void SetIsVisible(bool bVisible)
		{
			m_IsVisible = bVisible;
			Parent.SignalComponentChanged<Text2DComponent>(Notification::OnStateChanged);
		}

		const Ref<Font>& GetFont() const { return m_Font; }
		const std::string& GetText() const { return m_Text; }
		const glm::vec3& GetColor() const { return m_Color; }
		float GetLineSpacing() const { return m_LineSpacing; }
		float GetKerning() const { return m_Kerning; }
		float GetMaxWidth() const { return m_MaxWidth; }
		glm::vec2 GetPosition() const { return m_Pos; }
		glm::vec2 GetScale() const { return m_Scale; }
		float GetRotation() const { return m_Rotation; }
		float GetOpacity() const { return m_Opacity; }
		bool IsVisible() const { return m_IsVisible; }

	private:
		std::string m_Text = "Hello, 2D World!";
		Ref<Font> m_Font;

		glm::vec3 m_Color = glm::vec3(1.f);
		float m_LineSpacing = 0.0f;
		glm::vec2 m_Pos = glm::vec2{ 0.0f }; // Normalized device coords
		glm::vec2 m_Scale = glm::vec2{ 0.5f };
		float m_Rotation = 0.f;
		float m_Kerning = 0.0f;
		float m_MaxWidth = 10.0f;
		float m_Opacity = 1.f;
		bool m_IsVisible = true;
	};

	class CameraComponent : public SceneComponent
	{
	public:
		CameraComponent() = default;
		COMPONENT_DEFAULTS(CameraComponent);

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			CalculateViewMatrix();
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			CalculateViewMatrix();
		}

		glm::mat4 GetViewProjection() const
		{
			return Camera.GetProjection() * GetViewMatrix();
		}
		
		const glm::mat4& GetViewMatrix() const
		{
			return m_ViewMatrix;
		}
		
	private:
		void CalculateViewMatrix()
		{
			const glm::mat4 R = WorldTransform.Rotation.ToMat4();
			const glm::mat4 T = glm::translate(glm::mat4(1.0f), WorldTransform.Location);
			m_ViewMatrix = T * R;
			m_ViewMatrix = glm::inverse(m_ViewMatrix);
		}

	private:
		glm::mat4 m_ViewMatrix = glm::mat4(1.f);

	public:
		SceneCamera Camera;
		bool Primary = false; //TODO: think about moving to Scene, or somewhere else
		bool FixedAspectRatio = false;
	};

	class RigidBodyComponent : public SceneComponent
	{
	public:
		enum class Type { Static, Dynamic };
		enum class CollisionDetectionType { Discrete, Continuous, ContinuousSpeculative };

		RigidBodyComponent() = default;
		COMPONENT_DEFAULTS(RigidBodyComponent);

		void SetMass(float mass);
		float GetMass() const { return Mass; }

		void SetLinearDamping(float linearDamping);
		float GetLinearDamping() const { return LinearDamping; }

		void SetAngularDamping(float angularDamping);
		float GetAngularDamping() const { return AngularDamping; }

		void SetEnableGravity(bool bEnable);
		bool IsGravityEnabled() const { return bEnableGravity; }

		void SetIsKinematic(bool bKinematic);
		bool IsKinematic() const { return bKinematic; }
		
		void SetLockPosition(bool bLockX, bool bLockY, bool bLockZ);
		void SetLockPositionX(bool bLock);
		void SetLockPositionY(bool bLock);
		void SetLockPositionZ(bool bLock);

		void SetLockRotation(bool bLockX, bool bLockY, bool bLockZ);
		void SetLockRotationX(bool bLock);
		void SetLockRotationY(bool bLock);
		void SetLockRotationZ(bool bLock);

		bool IsPositionXLocked() const { return bLockPositionX; }
		bool IsPositionYLocked() const { return bLockPositionY; }
		bool IsPositionZLocked() const { return bLockPositionZ; }
		bool IsRotationXLocked() const { return bLockRotationX; }
		bool IsRotationYLocked() const { return bLockRotationY; }
		bool IsRotationZLocked() const { return bLockRotationZ; }

	public:
		Type BodyType = Type::Static;
		CollisionDetectionType CollisionDetection = CollisionDetectionType::Discrete;
	protected:
		float Mass = 1.f;
		float LinearDamping = 0.01f;
		float AngularDamping = 0.05f;
		bool bEnableGravity = false;
		bool bKinematic = false;
		
		bool bLockPositionX = false;
		bool bLockPositionY = false;
		bool bLockPositionZ = false;
		bool bLockRotationX = false;
		bool bLockRotationY = false;
		bool bLockRotationZ = false;
	};

	class BaseColliderComponent : public SceneComponent
	{
	public:
		virtual void SetIsTrigger(bool bTrigger) = 0;
		bool IsTrigger() const { return bTrigger; }

		virtual void SetPhysicsMaterial(const Ref<PhysicsMaterial>& material) = 0;
		const Ref<PhysicsMaterial>& GetPhysicsMaterial() const { return Material; }

		virtual void SetWorldTransform(const Transform& worldTransform) override;
		virtual void SetRelativeTransform(const Transform& relativeTransform) override;

		bool IsCollisionVisible() const { return bShowCollision; }
		virtual void SetShowCollision(bool bShowCollision) = 0;

	protected:
		BaseColliderComponent() = default;
		COMPONENT_DEFAULTS(BaseColliderComponent);
		virtual void UpdatePhysicsTransform() = 0;

	protected:
		Ref<PhysicsMaterial> Material = MakeRef<PhysicsMaterial>(0.6f, 0.6f, 0.5f);
		bool bTrigger = false;
		bool bShowCollision = false;
	};

	class BoxColliderComponent : public BaseColliderComponent
	{
	public:
		BoxColliderComponent() = default;
		BoxColliderComponent& operator=(const BoxColliderComponent& other)
		{
			BaseColliderComponent::operator=(other);
			SetSize(other.m_Size);
			SetPhysicsMaterial(Material);
			SetIsTrigger(other.bTrigger);
			SetShowCollision(other.bShowCollision);
			UpdatePhysicsTransform();

			return *this;
		}

		BoxColliderComponent(const BoxColliderComponent&) = delete;
		BoxColliderComponent(BoxColliderComponent&&) noexcept = default;
		BoxColliderComponent& operator=(BoxColliderComponent&&) noexcept = default;

		virtual void SetIsTrigger(bool bTrigger) override;
		virtual void SetPhysicsMaterial(const Ref<PhysicsMaterial>& material) override;
		virtual void SetShowCollision(bool bShowCollision) override;
		virtual void OnInit(Entity& entity) override;
		virtual void OnRemoved(Entity& entity) override;

		void SetSize(const glm::vec3& size);
		const glm::vec3& GetSize() const { return m_Size; }
	
	protected:
		virtual void UpdatePhysicsTransform() override;

	protected:
		Ref<BoxColliderShape> m_Shape;
		glm::vec3 m_Size = glm::vec3(1.f);
	};

	class SphereColliderComponent : public BaseColliderComponent
	{
	public:
		SphereColliderComponent() = default;
		SphereColliderComponent& operator=(const SphereColliderComponent& other)
		{ 
			BaseColliderComponent::operator=(other);
			SetRadius(other.Radius);
			SetPhysicsMaterial(Material);
			SetIsTrigger(other.bTrigger);
			SetShowCollision(other.bShowCollision);
			UpdatePhysicsTransform();

			return *this;
		}

		SphereColliderComponent(const SphereColliderComponent&) = delete;
		SphereColliderComponent(SphereColliderComponent&&) noexcept = default;
		SphereColliderComponent& operator=(SphereColliderComponent&&) noexcept = default;

		void SetRadius(float radius);
		float GetRadius() const { return Radius; }

		virtual void SetIsTrigger(bool bTrigger) override;
		virtual void SetPhysicsMaterial(const Ref<PhysicsMaterial>& material) override;
		virtual void SetShowCollision(bool bShowCollision) override;

		virtual void OnInit(Entity& entity) override;
		virtual void OnRemoved(Entity& entity) override;
	
	protected:
		virtual void UpdatePhysicsTransform() override;

	protected:
		Ref<SphereColliderShape> m_Shape;
		float Radius = 0.5f;
	};

	class CapsuleColliderComponent : public BaseColliderComponent
	{
	public:
		CapsuleColliderComponent() = default;
		CapsuleColliderComponent& operator=(const CapsuleColliderComponent& other)
		{
			BaseColliderComponent::operator=(other);
			SetHeightAndRadius(other.Height, other.Radius);
			SetPhysicsMaterial(Material);
			SetIsTrigger(other.bTrigger);
			SetShowCollision(other.bShowCollision);
			UpdatePhysicsTransform();

			return *this;
		}

		CapsuleColliderComponent(const CapsuleColliderComponent&) = delete;
		CapsuleColliderComponent(CapsuleColliderComponent&&) noexcept = default;
		CapsuleColliderComponent& operator=(CapsuleColliderComponent&&) noexcept = default;

		virtual void SetIsTrigger(bool bTrigger) override;
		virtual void SetPhysicsMaterial(const Ref<PhysicsMaterial>& material) override;
		virtual void SetShowCollision(bool bShowCollision) override;

		void SetHeight(float height)
		{
			SetHeightAndRadius(height, Radius);
		}
		float GetHeight() const { return Height; }

		void SetRadius(float radius)
		{
			SetHeightAndRadius(Height, radius);
		}
		float GetRadius() const { return Radius; }

		void SetHeightAndRadius(float height, float radius);

		virtual void OnInit(Entity& entity) override;
		virtual void OnRemoved(Entity& entity) override;

	protected:
		virtual void UpdatePhysicsTransform() override;

	protected:
		Ref<CapsuleColliderShape> m_Shape;
		float Radius = 0.5f;
		float Height = 1.f;
	};

	class MeshColliderComponent : public BaseColliderComponent
	{
	public:
		MeshColliderComponent() = default;
		MeshColliderComponent& operator=(const MeshColliderComponent& other)
		{
			BaseColliderComponent::operator=(other);
			SetCollisionMesh(other.CollisionMesh);
			SetPhysicsMaterial(Material);
			SetIsTrigger(other.bTrigger);
			SetShowCollision(other.bShowCollision);
			SetIsConvex(other.bConvex);
			UpdatePhysicsTransform();

			return *this;
		}

		MeshColliderComponent(const MeshColliderComponent&) = delete;
		MeshColliderComponent(MeshColliderComponent&&) noexcept = default;
		MeshColliderComponent& operator=(MeshColliderComponent&&) noexcept = default;

		virtual void SetIsTrigger(bool bTrigger) override;
		virtual void SetPhysicsMaterial(const Ref<PhysicsMaterial>& material) override;
		virtual void SetShowCollision(bool bShowCollision) override;

		void SetCollisionMesh(const Ref<StaticMesh>& mesh);
		const Ref<StaticMesh>& GetCollisionMesh() const { return CollisionMesh; }
		bool IsConvex() const { return bConvex; }
		void SetIsConvex(bool bConvex)
		{
			this->bConvex = bConvex;
			if (CollisionMesh) 
				SetCollisionMesh(CollisionMesh);
		}

		virtual void OnInit(Entity& entity) override;
		virtual void OnRemoved(Entity& entity) override;

	protected:
		virtual void UpdatePhysicsTransform() override;
	
	protected:
		Ref<MeshShape> m_Shape;
		Ref<StaticMesh> CollisionMesh;
		bool bConvex = true;
	};

	class ScriptComponent : public Component
	{
	public:
		ScriptComponent() = default;
		ScriptComponent(const std::string& moduleName) : Component(), ModuleName(moduleName) {}
		COMPONENT_DEFAULTS(ScriptComponent);

	public:
		std::unordered_map<std::string, PublicField> PublicFields;
		std::string ModuleName;
	};

	class NativeScriptComponent : public Component
	{
	public:
		NativeScriptComponent() = default;

		NativeScriptComponent(NativeScriptComponent&& other) noexcept 
		:Instance(other.Instance), 
		InitScript(other.InitScript), 
		DestroyScript(other.DestroyScript) 
		{
			other.Instance = nullptr; 
			other.InitScript = nullptr; 
			other.DestroyScript = nullptr; 
		}

		NativeScriptComponent& operator=(const NativeScriptComponent& other) 
		{ 
			if (this == &other)
				return *this;

			Instance = nullptr;
			InitScript = other.InitScript;
			DestroyScript = other.DestroyScript;

			return *this;
		}

		NativeScriptComponent& operator=(NativeScriptComponent&& other) noexcept 
		{ 
			Instance = other.Instance;
			InitScript = other.InitScript;
			DestroyScript = other.DestroyScript;

			other.Instance = nullptr;
			other.InitScript = nullptr;
			other.DestroyScript = nullptr;

			return *this;
		}

		template<typename T>
		void Bind()
		{
			EG_CORE_ASSERT(InitScript == nullptr, "Initialized Script twice!");
			InitScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}

	protected:
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InitScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		friend class Scene;
		friend void DestroyScript(NativeScriptComponent*);
	};

	class AudioComponent : public SceneComponent
	{
	public:
		AudioComponent() = default;
		AudioComponent& operator=(const AudioComponent& other)
		{
			Volume = other.Volume;
			LoopCount = other.LoopCount;
			bLooping = other.bLooping;
			bMuted = other.bMuted;
			bStreaming = other.bStreaming;
			MinDistance = other.MinDistance;
			MaxDistance = other.MaxDistance;
			RollOff = other.RollOff;
			bAutoplay = other.bAutoplay;
			bEnableDopplerEffect = other.bEnableDopplerEffect;

			SetSound(other.Sound);

			return *this;
		}

		AudioComponent(const AudioComponent&) = delete;
		AudioComponent(AudioComponent&&) noexcept = default;
		AudioComponent& operator=(AudioComponent&&) noexcept = default;

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			UpdateSoundPositionAndVelocity();
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			UpdateSoundPositionAndVelocity();
		}

		void SetMinDistance(float minDistance) { SetMinMaxDistance(minDistance, MaxDistance); }
		void SetMaxDistance(float maxDistance) { SetMinMaxDistance(MinDistance, maxDistance); }
		void SetMinMaxDistance(float minDistance, float maxDistance)
		{
			MinDistance = minDistance;
			MaxDistance = maxDistance;
			if (Sound)
				Sound->SetMinMaxDistance(MinDistance, MaxDistance);
		}
		float GetMinDistance() const { return MinDistance; }
		float GetMaxDistance() const { return MaxDistance; }

		void SetRollOffModel(RollOffModel rollOff)
		{
			RollOff = rollOff;
			if (Sound)
				Sound->SetRollOffModel(RollOff);
		}
		RollOffModel GetRollOffModel() const { return RollOff; }

		void SetVolume(float volume)
		{
			Volume = volume;
			if (Sound)
				Sound->SetVolume(volume);
		}
		float GetVolume() const { return Volume; }

		void SetLoopCount(int loopCount)
		{
			LoopCount = loopCount;
			if (Sound)
				Sound->SetLoopCount(loopCount);
		}
		int GetLoopCount() const { return LoopCount; }

		void SetLooping(bool bLooping)
		{
			this->bLooping = bLooping;
			if (Sound)
				Sound->SetLooping(bLooping);
		}
		bool IsLooping() const { return bLooping; }

		void SetMuted(bool bMuted)
		{
			this->bMuted = bMuted;
			if (Sound)
				Sound->SetMuted(bMuted);
		}
		bool IsMuted() const { return bMuted; }

		void SetSound(const Ref<Sound3D>& sound)
		{
			if (sound)
			{
				SoundSettings settings;
				settings.Volume = Volume;
				settings.LoopCount = LoopCount;
				settings.IsLooping = bLooping;
				settings.IsMuted = bMuted;
				settings.IsStreaming = bStreaming;
				Sound = Sound3D::Create(sound->GetSoundPath(), WorldTransform.Location, RollOff, settings);
				Sound->SetMinMaxDistance(MinDistance, MaxDistance);
			}
			else
				Sound = sound;
		}
		void SetSound(const Path& soundPath)
		{
			if (std::filesystem::exists(soundPath))
			{
				SoundSettings settings;
				settings.Volume = Volume;
				settings.LoopCount = LoopCount;
				settings.IsLooping = bLooping;
				settings.IsMuted = bMuted;
				settings.IsStreaming = bStreaming;
				Sound = Sound3D::Create(soundPath, WorldTransform.Location, RollOff, settings);
				Sound->SetMinMaxDistance(MinDistance, MaxDistance);
			}
			else
				Sound = nullptr;
		}
		const Ref<Sound3D> GetSound() const { return Sound; }

		void SetStreaming(bool bStreaming)
		{
			this->bStreaming = bStreaming;
			if (Sound)
				Sound->SetStreaming(bStreaming);
		}
		bool IsStreaming() const { return bStreaming; }

		void Play()
		{
			if (Sound)
				Sound->Play();
		}
		void Stop()
		{
			if (Sound)
				Sound->Stop();
		}
		void SetPaused(bool bPaused)
		{
			if (Sound)
				Sound->SetPaused(bPaused);
		}
		bool IsPlaying() const 
		{
			if (Sound)
				return Sound->IsPlaying();
			return false;
		}

	private:
		void UpdateSoundPositionAndVelocity()
		{
			if (Sound)
			{
				if (bEnableDopplerEffect)
					Sound->SetPositionAndVelocity(WorldTransform.Location, Parent.GetLinearVelocity());
				else
					Sound->SetPositionAndVelocity(WorldTransform.Location, glm::vec3{ 0.f });
			}
		}
	
	protected:
		Ref<Sound3D> Sound;
		float Volume = 1.f;
		int LoopCount = -1;
		bool bLooping = false;
		bool bMuted = false;
		bool bStreaming = false;
		float MinDistance = 1.f;
		float MaxDistance = 10000.f;
		RollOffModel RollOff = RollOffModel::Default;
	public:
		bool bAutoplay = true;
		bool bEnableDopplerEffect = false;
	};

	class ReverbComponent : public SceneComponent
	{
	public:
		ReverbComponent() = default;
		ReverbComponent& operator=(const ReverbComponent& other)
		{
			if (other.Reverb)
				Reverb = Reverb3D::Create(other.Reverb);

			return *this;
		}

		ReverbComponent(const ReverbComponent&) = delete;
		ReverbComponent(ReverbComponent&&) noexcept = default;
		ReverbComponent& operator=(ReverbComponent&&) noexcept = default;

		virtual void OnInit(Entity& entity) override
		{
			SceneComponent::OnInit(entity);
			Reverb->SetPosition(WorldTransform.Location);
		}

		void SetWorldTransform(const Transform& worldTransform) override
		{
			SceneComponent::SetWorldTransform(worldTransform);
			if (Reverb)
				Reverb->SetPosition(WorldTransform.Location);
		}

		void SetRelativeTransform(const Transform& relativeTransform) override
		{
			SceneComponent::SetRelativeTransform(relativeTransform);
			if (Reverb)
				Reverb->SetPosition(WorldTransform.Location);
		}

	public:
		Ref<Reverb3D> Reverb = Reverb3D::Create();
	};
}