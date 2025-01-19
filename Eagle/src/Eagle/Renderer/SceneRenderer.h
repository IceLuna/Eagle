#pragma once

#include "VidWrappers/Image.h"
#include "VidWrappers/Buffer.h"

#include "Tasks/RendererTask.h"
#include "Tasks/RenderLinesTask.h"
#include "Tasks/RenderTrianglesTask.h"
#include "Tasks/RenderBillboardsTask.h"
#include "Tasks/ShadowPassTask.h"
#include "Tasks/PBRPassTask.h"
#include "Tasks/LightsManagerTask.h"
#include "Tasks/GeometryManagerTask.h"
#include "Tasks/RenderTextUnlitTask.h"
#include "Tasks/RenderTextLitTask.h"
#include "Tasks/RenderText2DTask.h"
#include "Tasks/RenderImages2DTask.h"
#include "Tasks/RenderDecalsTask.h"
#include "Tasks/SSAOTask.h"
#include "Tasks/GTAOTask.h"
#include "Tasks/FogPassTask.h"

namespace Eagle
{
	class StaticMeshComponent;
	class SkeletalMeshComponent;
	class SpriteComponent;
	class BillboardComponent;
	class TextComponent;
	class Text2DComponent;
	class Image2DComponent;
	class ParticleSystemComponent;

	class PointLightComponent;
	class SpotLightComponent;
	class DirectionalLightComponent;

	class Texture2D;
	class StaticMesh;
	class Material;
	class AssetTextureCube;
	
	class Camera;

	class ParticleSystemTask;

	struct GBuffer
	{
		Ref<Image> Albedo;
		Ref<Image> Geometry_Shading_Normals;
		Ref<Image> Emissive;
		Ref<Image> MaterialData; // R: Metallness; G: AO; B: Roughness; A: Used for blending of material data when decals are used
		Ref<Image> Flags; // R: Flags. Currently, used for `bReceivesDecals`
		Ref<Image> ObjectID;
		Ref<Image> ObjectIDCopy;
		Ref<Image> Depth;
		Ref<Image> Motion;
		Ref<Image> DepthHistory;
		Ref<Image> NormalsHistory;

		void Init(const glm::uvec3& size);
		void InitOptional(const SceneRendererInternalState& optional, const glm::uvec3& size);
		void Resize(const glm::uvec3& size);
	};

	class SceneRenderer : public std::enable_shared_from_this<SceneRenderer>
	{
	public:
		SceneRenderer(const glm::uvec2 size, const SceneRendererSettings& options = {});

		void Render(const Camera* camera, const glm::mat4& viewMat, glm::vec3 viewPosition, glm::vec3 viewDirection);
		void SetOutputImage(const Ref<Image>& image);

		ImageSpecifications GetOutputImageSpecs() const; // Can be used to create an output image that can be used be the renderer (SetOutputImage)

		//---------------------------------- Render functions ----------------------------------
		// For these functions, Renderer copies required data from components
		// If 'bDirty' is false, passed data is ignored and last state is used to render.
		// Else buffers are cleared and required data from components is copied
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty) { m_GeometryManagerTask->SetMeshes(meshes, bDirty); }
		void SetSkeletalMeshes(const std::vector<SkeletalMeshComponent*>& meshes, bool bDirty) { m_GeometryManagerTask->SetSkeletalMeshes(meshes, bDirty); }
		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty) { m_GeometryManagerTask->SetSprites(sprites, bDirty); }
		void SetPointLights(const std::vector<const PointLightComponent*>& pointLights, bool bDirty) { m_LightsManagerTask->SetPointLights(pointLights, bDirty); }
		void SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights, bool bDirty) { m_LightsManagerTask->SetSpotLights(spotLights, bDirty); }
		void SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty) { m_GeometryManagerTask->SetTexts(texts, bDirty); }
		void SetTexts2D(const std::vector<const Text2DComponent*>& texts, bool bDirty) { m_Text2DTask->SetTexts(texts, bDirty); }
		void SetImages2D(const std::vector<const Image2DComponent*>& images, bool bDirty) { m_Images2DTask->SetImages(images, bDirty); }
		void SetDecals(const std::vector<const DecalComponent*>& decals, bool bDirty) { m_RenderDecalsTask->SetDecals(decals, bDirty); }
		void AddParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems);
		void UpdateParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems);
		void RemoveParticleSystems(const std::unordered_set<GUID>& systems); // GUIDs of ParticleSystemComponent: system->Parent.GetGUID(). It's done like that because we can't store a pointer to a dead component
		void UpdateParticleTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems);
		//--------------------------------------------------------------------------------------
		//---------------------------------- Render functions ----------------------------------
		void SetBillboards(const std::vector<const BillboardComponent*>& billboards) { m_RenderBillboardsTask->SetBillboards(billboards); }
		void SetDebugLines(const std::vector<RendererLine>& lines) { m_RenderLinesTask->SetDebugLines(lines); }
		void SetDebugTriangles(const std::vector<RendererTriangle>& triangles) { m_RenderTrianglesTask->SetDebugTriangles(triangles); }
		void AddAdditionalBillboard(const Transform& worldTransform, const Ref<Texture2D>& texture, int entityID = -1) { m_RenderBillboardsTask->AddAdditionalBillboard(worldTransform, texture, entityID); } // For internal usage

		// `directionalLight` can be set to nullptr to disable directional light
		void SetDirectionalLight(const DirectionalLightComponent* directionalLight) { m_LightsManagerTask->SetDirectionalLight(directionalLight); }

		// Instead of using `SetMeshes` and triggering all buffers recollection/uploading
		// This function can be used to update transforms of meshes that were already set
		void UpdateMeshesTransforms(const std::unordered_set<const StaticMeshComponent*>& meshes) { m_GeometryManagerTask->SetTransforms(meshes); }
		void UpdateSkeletalMeshesTransforms(const std::unordered_set<const SkeletalMeshComponent*>& meshes) { m_GeometryManagerTask->SetTransforms(meshes); }
		void UpdateSpritesTransforms(const std::unordered_set<const SpriteComponent*>& sprites) { m_GeometryManagerTask->SetTransforms(sprites); }
		void UpdateDecalsTransforms(const std::unordered_set<const DecalComponent*>& decals) { m_RenderDecalsTask->SetTransforms(decals); }
		void UpdateTextsTransforms(const std::unordered_set<const TextComponent*>& texts) { m_GeometryManagerTask->SetTransforms(texts); }

		void SetGridEnabled(bool bEnabled) { m_bGridEnabled = bEnabled; }

		void SetIsRuntime(bool bRuntime) { m_bIsRuntime = bRuntime; }
		bool IsRuntime() const { return m_bIsRuntime; }
		//--------------------------------------------------------------------------------------

		void SetSkybox(const Ref<AssetTextureCube>& cubemap);
		const Ref<AssetTextureCube>& GetSkybox() const { return m_Cubemap; }
		void SetSkyboxIntensity(float intensity);
		float GetSkyboxIntensity() const { return m_CubemapIntensity; }

		void SetSkybox(const SkySettings& sky);
		const SkySettings& GetSkySettings() const { return m_Sky; }

		// Also disables/enables sky (not just cubemap)
		void SetSkyboxEnabled(bool bEnabled) { m_bSkyboxEnabled = bEnabled; }
		bool IsSkyboxEnabled() const { return m_bSkyboxEnabled; }

		void SetUseSkyAsBackground(bool value);
		bool GetUseSkyAsBackground() const { return m_bUseSkyAsBackground; }

		void SetGravity(const glm::vec3& gravity) { m_Gravity = gravity; }
		glm::vec3 GetGravity() const { return m_Gravity; }

		void SetOptions(const SceneRendererSettings& options);
		void SetViewportSize(const glm::uvec2 size);
		glm::uvec2 GetViewportSize() const { return m_Size; }
		float GetAspectRatio() const { return float(m_Size.x) / float(m_Size.y); }

		// Key -> Mesh ID (entity ID)
		void SetMeshesAnimationTransforms(std::unordered_map<uint32_t, std::vector<glm::mat4>>&& transforms) { m_AnimationTransforms = std::move(transforms); }
		const std::unordered_map<uint32_t, std::vector<glm::mat4>>& GetMeshesAnimationTransforms() const { return m_AnimationTransforms; }

		// ----------- Getters from other tasks -----------
		// TODO: Implement a proper Render graph with input-output connections between tasks
		const auto& GetAllMeshes() const { return m_GeometryManagerTask->GetAllMeshes(); }
		const auto& GetOpaqueMeshes() const { return m_GeometryManagerTask->GetOpaqueMeshes(); }
		const auto& GetMaskedMeshes() const { return m_GeometryManagerTask->GetMaskedMeshes(); }
		const auto& GetTranslucentMeshes() const { return m_GeometryManagerTask->GetTranslucentMeshes(); }

		const auto& GetAllSkeletalMeshes() const { return m_GeometryManagerTask->GetAllSkeletalMeshes(); }
		const auto& GetOpaqueSkeletalMeshes() const { return m_GeometryManagerTask->GetOpaqueSkeletalMeshes(); }
		const auto& GetMaskedSkeletalMeshes() const { return m_GeometryManagerTask->GetMaskedSkeletalMeshes(); }
		const auto& GetTranslucentSkeletalMeshes() const { return m_GeometryManagerTask->GetTranslucentSkeletalMeshes(); }

		const auto& GetPointLights() const { return m_LightsManagerTask->GetPointLights(); }
		const auto& GetSpotLights() const { return m_LightsManagerTask->GetSpotLights(); }
		const auto& GetDirectionalLight() const { return m_LightsManagerTask->GetDirectionalLight(); }
		bool HasDirectionalLight() const { return m_LightsManagerTask->HasDirectionalLight(); }

		const Ref<Buffer>& GetPointLightsBuffer() const { return m_LightsManagerTask->GetPointLightsBuffer(); }
		const Ref<Buffer>& GetSpotLightsBuffer() const { return m_LightsManagerTask->GetSpotLightsBuffer(); }
		const Ref<Buffer>& GetDirectionalLightBuffer() const { return m_LightsManagerTask->GetDirectionalLightBuffer(); }

		const auto& GetOpaqueMeshesData() const { return m_GeometryManagerTask->GetOpaqueMeshesData(); }
		const auto& GetMaskedMeshesData() const { return m_GeometryManagerTask->GetMaskedMeshesData(); }
		const auto& GetTranslucentMeshesData() const { return m_GeometryManagerTask->GetTranslucentMeshesData(); }
		const Ref<Buffer>& GetMeshTransformsBuffer() const { return m_GeometryManagerTask->GetMeshesTransformBuffer(); }
		const Ref<Buffer>& GetMeshPrevTransformsBuffer() const { return m_GeometryManagerTask->GetMeshesPrevTransformBuffer(); }

		const auto& GetOpaqueSkeletalMeshesData() const { return m_GeometryManagerTask->GetOpaqueSkeletalMeshesData(); }
		const auto& GetMaskedSkeletalMeshesData() const { return m_GeometryManagerTask->GetMaskedSkeletalMeshesData(); }
		const auto& GetTranslucentSkeletalMeshesData() const { return m_GeometryManagerTask->GetTranslucentSkeletalMeshesData(); }
		const Ref<Buffer>& GetSkeletalMeshTransformsBuffer() const { return m_GeometryManagerTask->GetSkeletalMeshesTransformBuffer(); }
		const Ref<Buffer>& GetSkeletalMeshPrevTransformsBuffer() const { return m_GeometryManagerTask->GetSkeletalMeshesPrevTransformBuffer(); }
		const std::vector<std::vector<glm::mat4>>& GetAnimationTransforms() const { return m_GeometryManagerTask->GetAnimationTransforms(); }
		const std::vector<Ref<Buffer>>& GetAnimationTransformsBuffers() const { return m_GeometryManagerTask->GetAnimationTransformsBuffers(); }
		const std::vector<Ref<Buffer>>& GetAnimationPrevTransformsBuffers() const { return m_GeometryManagerTask->GetAnimationPrevTransformsBuffers(); }

		const auto& GetOpaqueSpritesData() const { return m_GeometryManagerTask->GetOpaqueSpriteData(); }
		const auto& GetOpaqueNotCastingShadowSpriteData() const { return m_GeometryManagerTask->GetOpaqueNotCastingShadowSpriteData(); }
		const auto& GetMaskedSpritesData() const { return m_GeometryManagerTask->GetMaskedSpriteData(); }
		const auto& GetMaskedNotCastingShadowSpriteData() const { return m_GeometryManagerTask->GetMaskedNotCastingShadowSpriteData(); }
		const auto& GetTranslucentSpritesData() const { return m_GeometryManagerTask->GetTranslucentSpriteData(); }
		const auto& GetTranslucentNotCastingShadowSpriteData() const { return m_GeometryManagerTask->GetTranslucentNotCastingShadowSpriteData(); }
		const Ref<Buffer>& GetSpritesTransformsBuffer() const { return m_GeometryManagerTask->GetSpritesTransformBuffer(); }
		const Ref<Buffer>& GetSpritesPrevTransformBuffer() const { return m_GeometryManagerTask->GetSpritesPrevTransformBuffer(); }

		const LitTextGeometryData& GetOpaqueLitTextData() const { return m_GeometryManagerTask->GetOpaqueLitTextData(); }
		const LitTextGeometryData& GetOpaqueLitNotCastingShadowTextData() const { return m_GeometryManagerTask->GetOpaqueLitNotCastingShadowTextData(); }
		const LitTextGeometryData& GetMaskedLitTextData() const { return m_GeometryManagerTask->GetMaskedLitTextData(); }
		const LitTextGeometryData& GetMaskedLitNotCastingShadowTextData() const { return m_GeometryManagerTask->GetMaskedLitNotCastingShadowTextData(); }
		const LitTextGeometryData& GetTranslucentLitTextData() const { return m_GeometryManagerTask->GetTranslucentLitTextData(); }
		const LitTextGeometryData& GetTranslucentLitNotCastingShadowTextData() const { return m_GeometryManagerTask->GetTranslucentLitNotCastingShadowTextData(); }
		const UnlitTextGeometryData& GetUnlitTextData() const { return m_GeometryManagerTask->GetUnlitTextData(); }
		const UnlitTextGeometryData& GetUnlitNotCastingShadowTextData() const { return m_GeometryManagerTask->GetUnlitNotCastingShadowTextData(); }
		const Ref<Buffer>& GetTextsTransformsBuffer() const { return m_GeometryManagerTask->GetTextsTransformBuffer(); }
		const Ref<Buffer>& GetTextsPrevTransformBuffer() const { return m_GeometryManagerTask->GetTextsPrevTransformBuffer(); }
		const std::vector<Ref<Texture2D>>& GetAtlases() const { return m_GeometryManagerTask->GetAtlases(); }

		const std::vector<Ref<Image>>& GetPointLightShadowMaps() const { return m_ShadowPassTask->GetPointLightShadowMaps(); }
		const std::vector<Ref<Image>>& GetPointLightShadowMapsColored() const { return m_ShadowPassTask->GetPointLightShadowMapsColored(); }
		const std::vector<Ref<Image>>& GetPointLightShadowMapsColoredDepth() const { return m_ShadowPassTask->GetPointLightShadowMapsColoredDepth(); }

		const std::vector<Ref<Image>>& GetSpotLightShadowMaps() const { return m_ShadowPassTask->GetSpotLightShadowMaps(); }
		const std::vector<Ref<Image>>& GetSpotLightShadowMapsColored() const { return m_ShadowPassTask->GetSpotLightShadowMapsColored(); }
		const std::vector<Ref<Image>>& GetSpotLightShadowMapsColoredDepth() const { return m_ShadowPassTask->GetSpotLightShadowMapsColoredDepth(); }

		const std::vector<Ref<Image>>& GetDirectionalLightShadowMaps() const { return m_ShadowPassTask->GetDirectionalLightShadowMaps(); }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMapsColored() const { return m_ShadowPassTask->GetDirectionalLightShadowMapsColored(); }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMapsColoredDepth() const { return m_ShadowPassTask->GetDirectionalLightShadowMapsColoredDepth(); }

		const std::vector<Ref<Sampler>>& GetPointLightShadowMapsSamplers() const { return m_ShadowPassTask->GetPointLightShadowMapsSamplers(); }
		const std::vector<Ref<Sampler>>& GetSpotLightShadowMapsSamplers() const { return m_ShadowPassTask->GetSpotLightShadowMapsSamplers(); }
		const std::vector<Ref<Sampler>>& GetDirectionalLightShadowMapsSamplers() const { return m_ShadowPassTask->GetDirectionalLightShadowMapsSamplers(); }

		const Ref<Buffer>& GetFogDataBuffer() const { return m_FogTask->GetFogDataBuffer(); }

		// Contains View Matrix, InvVP
		const Ref<Buffer>& GetCameraBuffer() const { return m_CameraDataBuffer; }
		const Ref<Image>& GetSMDistribution() const { return m_PBRPassTask->GetSMDistribution(); }

		const Ref<Image>& GetSSAOResult() const { return m_SSAOTask->GetResult(); }
		const Ref<Image>& GetGTAOResult() const { return m_GTAOTask->GetResult(); }
		// ------------------------------------------------

		const Ref<Buffer>& GetJitter() const { return m_Jitter; }
		const GBuffer& GetGBuffer() const { return m_GBuffer; }
		GBuffer& GetGBuffer() { return m_GBuffer; }
		const Ref<Image>& GetOutput() const { return m_FinalImage; }
		const Ref<Image>& GetHDROutput() const { return m_HDRRTImage; }

		const SceneRendererSettings& GetOptions_RT() const { return m_Options_RT; }
		const SceneRendererSettings& GetOptions() const { return m_Options; }
		const glm::mat4& GetViewMatrix() const { return m_View; }
		const glm::mat4& GetProjectionMatrix() const { return m_Projection; }
		const glm::mat4& GetViewProjection() const { return m_ViewProjection; }
		const glm::mat4& GetInverseViewProjection() const { return m_InvViewProjection; }
		const glm::vec3 GetViewPosition() const { return m_ViewPos; }
		const glm::vec3 GetViewDirection() const { return m_ViewDir; }
		float GetPhotoLinearScale() const { return m_PhotoLinearScale; }
		float GetZNear() const { return m_ZNear; }
		float GetZFar() const { return m_ZFar; }
		float GetFOV() const { return m_CameraFOV; }

		// Prev frame data
		const glm::mat4& GetPrevViewMatrix() const { return m_PrevView; }
		const glm::mat4& GetPrevProjectionMatrix() const { return m_PrevProjection; }
		const glm::mat4& GetPrevViewProjection() const { return m_PrevViewProjection; }

		const std::vector<glm::mat4>& GetCascadeProjections() const { return m_CameraCascadeProjections; }
		const std::vector<float>& GetCascadeFarPlanes() const { return m_CameraCascadeFarPlanes; }
		float GetShadowMaxDistance() const { return m_MaxShadowDistance; }

	public:
		//Stats
		struct Statistics
		{
			uint64_t DrawCalls = 0;
			uint64_t Vertices = 0;
			uint64_t Indeces = 0;
		};

		struct Statistics2D
		{
			uint64_t DrawCalls = 0;
			uint64_t QuadCount = 0;

			inline uint64_t GetVertexCount() const { return QuadCount * 4; }
			inline uint64_t GetIndexCount() const { return QuadCount * 6; }
		};

		Statistics& GetStats() { return m_Stats[m_FrameIndex]; }
		Statistics2D& GetStats2D() { return m_Stats2D[m_FrameIndex]; }

		const Statistics& GetStats() const { return m_Stats[m_FrameIndex]; }
		const Statistics2D& GetStats2D() const { return m_Stats2D[m_FrameIndex]; }

	private:
		void InitWithOptions();

	private:
		Ref<GeometryManagerTask> m_GeometryManagerTask;
		Ref<RendererTask> m_RenderMeshesTask;
		Ref<RendererTask> m_RenderSkeletalMeshesTask;
		Ref<RendererTask> m_RenderSpritesTask;
		Ref<RenderDecalsTask> m_RenderDecalsTask;
		Ref<RenderTextLitTask> m_RenderLitTextTask;
		Ref<RenderTextUnlitTask> m_RenderUnlitTextTask;
		Ref<LightsManagerTask> m_LightsManagerTask;
		Ref<RenderLinesTask> m_RenderLinesTask;
		Ref<RenderTrianglesTask> m_RenderTrianglesTask;
		Ref<RendererTask> m_TAATask;
		Ref<RenderBillboardsTask> m_RenderBillboardsTask;
		Ref<PBRPassTask> m_PBRPassTask;
		Ref<ShadowPassTask> m_ShadowPassTask;
		Ref<RendererTask> m_BloomTask;
		Ref<RendererTask> m_SkyboxPassTask;
		Ref<RendererTask> m_PostProcessingPassTask;
		Ref<SSAOTask> m_SSAOTask;
		Ref<GTAOTask> m_GTAOTask;
		Ref<RendererTask> m_GridTask;
		Ref<RendererTask> m_TransparencyTask;
		Ref<RenderText2DTask> m_Text2DTask;
		Ref<RenderImages2DTask> m_Images2DTask;
		Ref<RendererTask> m_VolumetricTask;
		Ref<FogPassTask> m_FogTask;
		Ref<ParticleSystemTask> m_ParticleTask;
		Ref<RendererTask> m_DOFTask;
		Ref<RendererTask> m_MotionBlurTask;
		Ref<RendererTask> m_ScreenSpaceReflectionsTask;
		
		Ref<Buffer> m_Jitter;
		Ref<Buffer> m_CameraDataBuffer;

		GBuffer m_GBuffer;
		Ref<Image> m_FinalImage;
		Ref<Image> m_HDRRTImage; // Render target
		Ref<AssetTextureCube> m_Cubemap;
		float m_CubemapIntensity = 1.f;
		bool m_bSkyboxEnabled = true;

		std::unordered_map<uint32_t, std::vector<glm::mat4>> m_AnimationTransforms;

		SkySettings m_Sky;
		glm::mat4 m_View = glm::mat4(1.f);
		glm::mat4 m_Projection = glm::mat4(1.f);
		glm::mat4 m_ViewProjection = glm::mat4(1.f);
		glm::mat4 m_InvViewProjection = glm::mat4(1.f);
		glm::vec3 m_ViewPos = glm::vec3(0.f);
		glm::vec3 m_ViewDir = glm::vec3(0.f);

		// Prev frame data
		glm::mat4 m_PrevView = glm::mat4(1.f);
		glm::mat4 m_PrevProjection = glm::mat4(1.f);
		glm::mat4 m_PrevViewProjection = glm::mat4(1.f);

		std::vector<glm::mat4> m_CameraCascadeProjections = std::vector<glm::mat4>(EG_CASCADES_COUNT);
		std::vector<float> m_CameraCascadeFarPlanes = std::vector<float>(EG_CASCADES_COUNT);
		float m_MaxShadowDistance = 1.f;

		glm::uvec2 m_Size = { 1, 1 };
		float m_PhotoLinearScale = 1.f;
		float m_ZNear = 1.f;
		glm::vec3 m_Gravity = glm::vec3(0);
		float m_ZFar = 1.f;
		float m_CameraFOV = 1.f;
		SceneRendererSettings m_Options_RT; // Render thread
		SceneRendererSettings m_Options;

		uint32_t m_FrameIndex = 0;

		bool m_bGridEnabled = false;
		bool m_bUseSkyAsBackground = true;
		bool m_bIsRuntime = false;
		bool m_bIsGame = false;

		Statistics m_Stats[RendererConfig::FramesInFlight];
		Statistics2D m_Stats2D[RendererConfig::FramesInFlight];
	};
}
