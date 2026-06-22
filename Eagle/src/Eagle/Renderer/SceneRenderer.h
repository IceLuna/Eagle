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
#include "Tasks/SkinCacheTask.h"
#include "Tasks/FrustumCullingTask.h"
#include "Tasks/LightCullingTask.h"
#include "Tasks/ScreenSpaceShadowsTask.h"

#include <ankerl/unordered_dense.h>

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
		Ref<Image> Normals; // Packed Geometry Normals are stored in XY; Packed Shading Normals are stored in ZW
		Ref<Image> Emissive;
		Ref<Image> MaterialData; // R: Metallness; G: AO; B: Roughness; A: Used for blending of material data when decals are used
		Ref<Image> Flags; // R: Flags. Currently, used for `bReceivesDecals`
		Ref<Image> ObjectID;
		Ref<Image> ObjectIDCopy;
		Ref<Image> Depth;
		Ref<Image> Motion;

		void Init(const glm::uvec3& size);
		void InitOptional(const SceneRendererInternalState& optional, const glm::uvec3& size);
		void Resize(const glm::uvec3& size);
		void Clear(const Ref<CommandBuffer>& cmd);
		void PrepareForReading(const Ref<CommandBuffer>& cmd);
	};

	struct CameraData
	{
		glm::mat4 View;
		glm::mat4 InvViewProj;
		glm::mat4 ViewProj;
		glm::mat4 PrevViewProj;
		glm::mat4 Proj;
		glm::mat4 InvProj;
		glm::mat4 PrevProj;
		glm::mat4 PrevView;
		glm::mat4 ViewProjUnjittered;
		glm::mat4 PrevViewProjUnjittered;
		glm::mat4 ProjUnjittered;
		glm::mat4 PrevProjUnjittered;
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
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes) { m_GeometryManagerTask->SetMeshes(meshes); }
		void SetSkeletalMeshes(const std::vector<SkeletalMeshComponent*>& meshes) { m_GeometryManagerTask->SetSkeletalMeshes(meshes); }
		void SetSprites(const std::vector<const SpriteComponent*>& sprites) { m_GeometryManagerTask->SetSprites(sprites); }
		void SetPointLights(const std::vector<const PointLightComponent*>& pointLights) { m_LightsManagerTask->SetPointLights(pointLights); }
		void SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights) { m_LightsManagerTask->SetSpotLights(spotLights); }
		void SetTexts(const std::vector<const TextComponent*>& texts) { m_GeometryManagerTask->SetTexts(texts); }
		void SetTexts2D(const std::vector<const Text2DComponent*>& texts) { m_Text2DTask->SetTexts(texts); }
		void SetImages2D(const std::vector<const Image2DComponent*>& images) { m_Images2DTask->SetImages(images); }
		void SetDecals(const std::vector<const DecalComponent*>& decals) { m_RenderDecalsTask->SetDecals(decals); }
		void AddParticleSystem(const ParticleSystemComponent& system);
		void UpdateParticleSystem(const ParticleSystemComponent& system);
		void RemoveParticleSystem(const ParticleSystemComponent& system);
		void UpdateParticleTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems);
		void RemoveAllParticleSystems();
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
		void UpdateMeshesTransforms(const std::vector<const StaticMeshComponent*>& meshes) { m_GeometryManagerTask->SetTransforms(meshes); }
		void UpdateSkeletalMeshesTransforms(const std::vector<const SkeletalMeshComponent*>& meshes) { m_GeometryManagerTask->SetTransforms(meshes); }
		void UpdateSpritesTransforms(const std::vector<const SpriteComponent*>& sprites) { m_GeometryManagerTask->SetTransforms(sprites); }
		void UpdateDecalsTransforms(const std::vector<const DecalComponent*>& decals) { m_RenderDecalsTask->SetTransforms(decals); }
		void UpdateTextsTransforms(const std::vector<const TextComponent*>& texts) { m_GeometryManagerTask->SetTransforms(texts); }

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
		void SetRenderSkybox(bool bEnabled) { m_bRenderSkybox = bEnabled; }
		bool IsRenderSkyboxEnabled() const { return m_bRenderSkybox; }

		void SetUseSkyAsBackground(bool value);
		bool GetUseSkyAsBackground() const { return m_bUseSkyAsBackground; }

		void SetGravity(const glm::vec3& gravity) { m_Gravity = gravity; }
		glm::vec3 GetGravity() const { return m_Gravity; }

		void SetOptions(const SceneRendererSettings& options);
		void SetViewportSize(const glm::uvec2 size);
		glm::uvec2 GetViewportSize() const { return m_Size; }
		float GetAspectRatio() const { return float(m_Size.x) / float(m_Size.y); }

		// Key -> Mesh ID (entity ID)
		void SetMeshesAnimationTransforms(ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>>&& transforms);
		const ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>>& GetMeshesAnimationTransforms_RT() const { return m_AnimationTransforms; }

		// Key - system ID; Value - transforms per emitter
		void SetSkeletalParticleAnimationTransforms(ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>>&& transforms);
		const auto& GetSkeletalParticleAnimationTransforms_RT() const { return m_SkeletalParticlesAnimationTransforms; }

		// ----------- Getters from other tasks -----------
		// TODO: Implement a proper Render graph with input-output connections between tasks
		const auto& GetStaticMeshesDrawData() const { return m_GeometryManagerTask->GetStaticMeshesDrawData(); }
		const auto& GetStaticMeshesBuffers() const { return m_GeometryManagerTask->GetStaticMeshesBuffers(); }

		const auto& GetSkeletalMeshesDrawData() const { return m_GeometryManagerTask->GetSkeletalMeshesDrawData(); }
		const auto& GetSkeletalMeshesBuffers() const { return m_GeometryManagerTask->GetSkeletalMeshesBuffers(); }
		const auto& GetSkeletalMeshes() const { return m_GeometryManagerTask->GetSkeletalMeshes(); }
		const auto& GetSkinnedVertices() const { return m_SkinCacheTask->GetSkinnedVertices(); }
		const auto& GetSkinnedAABBs() const { return m_SkinCacheTask->GetSkinnedAABBs(); }
		const auto& GetPrevSkinnedVerticesPositions() const { return m_SkinCacheTask->GetPrevSkinnedVerticesPositions(); }

		const auto& GetFrustumCullingTask() const { return m_FrustumCullingTask; }
		const auto& GetCulledStaticMeshes() const { return m_FrustumCullingTask->GetCulledStaticMeshes(); }
		const auto& GetCulledSkeletalMeshes() const { return m_FrustumCullingTask->GetCulledSkeletalMeshes(); }

		const auto& GetLightCullingTask() const { return m_LightCullingTask; }

		const auto& GetPointLights() const { return m_LightsManagerTask->GetPointLights(); }
		const auto& GetSpotLights() const { return m_LightsManagerTask->GetSpotLights(); }
		const auto& GetCulledPointLights() const { return m_LightCullingTask->GetCulledPointLights(); }
		const auto& GetCulledSpotLights() const { return m_LightCullingTask->GetCulledSpotLights(); }
		const auto& GetDirectionalLight() const { return m_LightsManagerTask->GetDirectionalLight(); }
		const auto& GetLightMatrices() const { return m_LightsManagerTask->GetLightMatrices(); }
		const auto& GetPointLightMatrices() const { return m_LightsManagerTask->GetPointLightMatrices(); }
		bool HasDirectionalLight() const { return m_LightsManagerTask->HasDirectionalLight(); }
		bool HasVolumetricLights() const { return m_LightsManagerTask->HasVolumetricLights(); }

		const Ref<Buffer>& GetPointLightsBuffer() const { return m_LightsManagerTask->GetPointLightsBuffer(); }
		const Ref<Buffer>& GetSpotLightsBuffer() const { return m_LightsManagerTask->GetSpotLightsBuffer(); }
		const Ref<Buffer>& GetDirectionalLightBuffer() const { return m_LightsManagerTask->GetDirectionalLightBuffer(); }
		const Ref<Buffer>& GetLightMatricesBuffer() const { return m_LightsManagerTask->GetLightMatricesBuffer(); }

		const Ref<Buffer>& GetMeshTransformsBuffer() const { return m_GeometryManagerTask->GetMeshesTransformBuffer(); }
		const Ref<Buffer>& GetMeshPrevTransformsBuffer() const { return m_GeometryManagerTask->GetMeshesPrevTransformBuffer(); }

		const Ref<Buffer>& GetSkeletalMeshTransformsBuffer() const { return m_GeometryManagerTask->GetSkeletalMeshesTransformBuffer(); }
		const std::vector<std::vector<glm::mat4>>& GetAnimationTransforms() const { return m_GeometryManagerTask->GetAnimationTransforms(); }
		const std::vector<Ref<Buffer>>& GetAnimationTransformsBuffers() const { return m_GeometryManagerTask->GetAnimationTransformsBuffers(); }

		const auto& GetSingleSidedSpritesRenderData() const { return m_GeometryManagerTask->GetSingleSidedSpritesRenderData(); }
		const auto& GetDoubleSidedSpritesRenderData() const { return m_GeometryManagerTask->GetDoubleSidedSpritesRenderData(); }
		const Ref<Buffer>& GetSpritesTransformsBuffer() const { return m_GeometryManagerTask->GetSpritesTransformBuffer(); }
		const Ref<Buffer>& GetSpritesPrevTransformBuffer() const { return m_GeometryManagerTask->GetSpritesPrevTransformBuffer(); }

		const auto& GetSingleSidedTextsRenderData() const { return m_GeometryManagerTask->GetSingleSidedTextsRenderData(); }
		const auto& GetDoubleSidedTextsRenderData() const { return m_GeometryManagerTask->GetDoubleSidedTextsRenderData(); }
		const auto& GetSingleSidedUnlitTextsRenderData() const { return m_GeometryManagerTask->GetSingleSidedUnlitTextsRenderData(); }
		const auto& GetDoubleSidedUnlitTextsRenderData() const { return m_GeometryManagerTask->GetDoubleSidedUnlitTextsRenderData(); }
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

		const Ref<Sampler>& GetShadowMapPCFSampler() const { return m_ShadowPassTask->GetPCFSampler(); }
		const Ref<Sampler>& GetShadowMapPointSampler() const { return m_ShadowPassTask->GetPointSampler(); }
		const Ref<Sampler>& GetColoredShadowMapSampler() const { return m_ShadowPassTask->GetColoredShadowMapsSampler(); }

		const Ref<Buffer>& GetFogDataBuffer() const { return m_FogTask->GetFogDataBuffer(); }

		// Contains View Matrix, InvVP
		const Ref<Buffer>& GetCameraMatricesBuffer() const { return m_CameraDataBuffer; }
		const Ref<Image>& GetSMDistribution() const { return m_PBRPassTask->GetSMDistribution(); }

		Ref<Image> GetSSAOResult() const { return m_SSAOTask ? m_SSAOTask->GetResult() : nullptr; }
		Ref<Image> GetGTAOResult() const { return m_GTAOTask ? m_GTAOTask->GetResult() : nullptr; }
		Ref<Image> GetGTAOBentNormals() const { return m_GTAOTask ? m_GTAOTask->GetBentNormals() : nullptr; }
		const Ref<Image>& GetScreenSpaceShadows() const { return m_ScreenSpaceShadows->GetResult(); }
		// ------------------------------------------------

		const GBuffer& GetGBuffer() const { return m_GBuffer; }
		GBuffer& GetGBuffer() { return m_GBuffer; }
		const Ref<Image>& GetOutput() const { return m_FinalImage; }
		const Ref<Image>& GetHDROutput() const { return m_HDRRTImage; }

		const SceneRendererSettings& GetOptions_RT() const { return m_Options_RT; }
		const SceneRendererSettings& GetOptions() const { return m_Options; }
		const CameraData& GetCameraMatrices() const { return m_CameraMatrices; }
		const glm::mat4& GetViewMatrix() const { return m_CameraMatrices.View; }
		const glm::mat4& GetProjectionMatrix() const { return m_CameraMatrices.Proj; }
		const glm::mat4& GetViewProjection() const { return m_CameraMatrices.ViewProj; }
		const glm::mat4& GetInverseViewProjection() const { return m_CameraMatrices.InvViewProj; }
		const glm::vec3 GetViewPosition() const { return m_ViewPos; }
		const glm::vec3 GetViewDirection() const { return m_ViewDir; }
		float GetPhotoLinearScale() const { return m_PhotoLinearScale; }
		float GetZNear() const { return m_ZNear; }
		float GetZFar() const { return m_ZFar; }
		float GetFOV() const { return m_CameraFOV; }

		// Needs to be called every frame
		void SetDebugFrustumCulling(const glm::vec3& cameraPos, const glm::mat4& view, const Camera& camera, float aspectRatio);
		const CullingFrustumData& GetCullingFrustumData() const { return m_CullingData; }

		// Prev frame data
		const glm::mat4& GetPrevViewMatrix() const { return m_PrevView; }
		const glm::mat4& GetPrevProjectionMatrix() const { return m_PrevProjection; }
		const glm::mat4& GetPrevViewProjection() const { return m_PrevViewProjection; }

		const std::vector<glm::mat4>& GetCascadeProjections() const { return m_CameraCascadeProjections; }
		const std::vector<float>& GetCascadeFarPlanes() const { return m_CameraCascadeFarPlanes; }
		float GetShadowMaxDistance() const { return m_MaxShadowDistance; }

		float IsProjectionFlipped() const { return m_bProjectionFlipped; }

	public:
		RenderStats& GetStats() { return m_Stats[m_FrameIndex]; }
		const RenderStats& GetStats() const { return m_Stats[m_FrameIndex]; }
		const RenderStats& GetStats_MT() const { return m_Stats_MT; }

	private:
		void InitWithOptions();

	private:
		Ref<SkinCacheTask> m_SkinCacheTask;
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
		Ref<RendererTask> m_MSAATask;
		Ref<RendererTask> m_FXAATask;
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
		Ref<FrustumCullingTask> m_FrustumCullingTask;
		Ref<LightCullingTask> m_LightCullingTask;
		Ref<ScreenSpaceShadowsTask> m_ScreenSpaceShadows;
		
		Ref<Buffer> m_CameraDataBuffer;
		
		CullingFrustumData m_CullingData;
		// If set, these values will be used for frustum culling for debug/visualization purposes
		CullingFrustumData m_DebugCullingData;
		bool m_bUseDebugCullingFrustum = false;

		GBuffer m_GBuffer;
		Ref<Image> m_FinalImage;
		Ref<Image> m_HDRRTImage; // Render target
		Ref<AssetTextureCube> m_Cubemap;
		float m_CubemapIntensity = 1.f;
		bool m_bSkyboxEnabled = true;
		bool m_bRenderSkybox = true;

		ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> m_AnimationTransforms;
		ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>> m_SkeletalParticlesAnimationTransforms;

		SkySettings m_Sky;
		CameraData m_CameraMatrices;
		glm::vec3 m_ViewPos = glm::vec3(0.f);
		glm::vec3 m_ViewDir = glm::vec3(0.f);

		// Prev frame data
		glm::mat4 m_PrevView = glm::mat4(1.f);
		glm::mat4 m_PrevProjection = glm::mat4(1.f);
		glm::mat4 m_PrevViewProjection = glm::mat4(1.f);

		std::vector<glm::mat4> m_CameraCascadeProjections = std::vector<glm::mat4>(EG_CASCADES_COUNT);
		std::vector<float> m_CameraCascadeFarPlanes = std::vector<float>(EG_CASCADES_COUNT);
		float m_MaxShadowDistance = 1.f;
		bool m_bProjectionFlipped = true;

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

		RenderStats m_Stats[RendererConfig::FramesInFlight];
		RenderStats m_Stats_MT{};
	};
}
