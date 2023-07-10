#pragma once

#include "VidWrappers/Image.h"
#include "VidWrappers/Buffer.h"

#include "Tasks/RendererTask.h"
#include "Tasks/RenderLinesTask.h"
#include "Tasks/RenderBillboardsTask.h"
#include "Tasks/ShadowPassTask.h"
#include "Tasks/PBRPassTask.h"
#include "Tasks/LightsManagerTask.h"
#include "Tasks/GeometryManagerTask.h"
#include "Tasks/RenderTextUnlitTask.h"
#include "Tasks/RenderTextLitTask.h"
#include "Tasks/SSAOTask.h"
#include "Tasks/GTAOTask.h"

namespace Eagle
{
	class StaticMeshComponent;
	class SpriteComponent;
	class BillboardComponent;
	class TextComponent;

	class PointLightComponent;
	class SpotLightComponent;
	class DirectionalLightComponent;

	class Texture2D;
	class StaticMesh;
	class Material;
	class TextureCube;
	
	class Camera;

	struct GBuffer
	{
		Ref<Image> AlbedoRoughness; // Albedo Roughness
		Ref<Image> Geometry_Shading_Normals;
		Ref<Image> Emissive;
		Ref<Image> MaterialData; // R: Metallness; G: AO
		Ref<Image> ObjectID;
		Ref<Image> Depth;
		Ref<Image> Motion;

		void Resize(const glm::uvec3& size)
		{
			AlbedoRoughness->Resize(size);
			MaterialData->Resize(size);
			Geometry_Shading_Normals->Resize(size);
			Emissive->Resize(size);
			ObjectID->Resize(size);
			Depth->Resize(size);
			if (Motion)
				Motion->Resize(size);
		}

		void Init(const glm::uvec3& size);
		void InitOptional(const OptionalGBuffers& optional, const glm::uvec3& size);
	};

	class SceneRenderer : public std::enable_shared_from_this<SceneRenderer>
	{
	public:
		SceneRenderer(const glm::uvec2 size, const SceneRendererSettings& options = {});

		void Render(const Camera* camera, const glm::mat4& viewMat, glm::vec3 viewPosition);

		//---------------------------------- Render functions ----------------------------------
		// For these functions, Renderer copies required data from components
		// If 'bDirty' is false, passed data is ignored and last state is used to render.
		// Else buffers are cleared and required data from components is copied
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty) { m_GeometryManagerTask->SetMeshes(meshes, bDirty); }
		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty) { m_GeometryManagerTask->SetSprites(sprites, bDirty); }
		void SetPointLights(const std::vector<const PointLightComponent*>& pointLights, bool bDirty) { m_LightsManagerTask->SetPointLights(pointLights, bDirty); }
		void SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights, bool bDirty) { m_LightsManagerTask->SetSpotLights(spotLights, bDirty); }
		void SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty) { m_GeometryManagerTask->SetTexts(texts, bDirty); }
		//--------------------------------------------------------------------------------------
		//---------------------------------- Render functions ----------------------------------
		void SetBillboards(const std::vector<const BillboardComponent*>& billboards) { m_RenderBillboardsTask->SetBillboards(billboards); }
		void SetDebugLines(const std::vector<RendererLine>& lines) { m_RenderLinesTask->SetDebugLines(lines); }
		void AddAdditionalBillboard(const Transform& worldTransform, const Ref<Texture2D>& texture, int entityID = -1) { m_RenderBillboardsTask->AddAdditionalBillboard(worldTransform, texture, entityID); } // For internal usage

		// `directionalLight` can be set to nullptr to disable directional light
		void SetDirectionalLight(const DirectionalLightComponent* directionalLight) { m_LightsManagerTask->SetDirectionalLight(directionalLight); }

		// Instead of using `SetMeshes` and triggering all buffers recollection/uploading
		// This function can be used to update transforms of meshes that were already set
		void UpdateMeshesTransforms(const std::set<const StaticMeshComponent*>& meshes) { m_GeometryManagerTask->SetTransforms(meshes); }
		void UpdateSpritesTransforms(const std::set<const SpriteComponent*>& sprites) { m_GeometryManagerTask->SetTransforms(sprites); }
		void UpdateTextsTransforms(const std::set<const TextComponent*>& texts) { m_GeometryManagerTask->SetTransforms(texts); }

		void SetGridEnabled(bool bEnabled) { m_bGridEnabled = bEnabled; }
		//--------------------------------------------------------------------------------------

		void SetSkybox(const Ref<TextureCube>& cubemap);
		const Ref<TextureCube>& GetSkybox() const { return m_Cubemap; }

		void SetOptions(const SceneRendererSettings& options);
		void SetViewportSize(const glm::uvec2 size);
		glm::uvec2 GetViewportSize() const { return m_Size; }
		float GetAspectRatio() const { return float(m_Size.x) / float(m_Size.y); }

		// ----------- Getters from other tasks -----------
		// TODO: Implement a proper Render graph with input-output connections between tasks
		const auto& GetAllMeshes() const { return m_GeometryManagerTask->GetAllMeshes(); }
		const auto& GetOpaqueMeshes() const { return m_GeometryManagerTask->GetOpaqueMeshes(); }
		const auto& GetTranslucentMeshes() const { return m_GeometryManagerTask->GetTranslucentMeshes(); }

		const auto& GetPointLights() const { return m_LightsManagerTask->GetPointLights(); }
		const auto& GetSpotLights() const { return m_LightsManagerTask->GetSpotLights(); }
		const auto& GetDirectionalLight() const { return m_LightsManagerTask->GetDirectionalLight(); }
		bool HasDirectionalLight() const { return m_LightsManagerTask->HasDirectionalLight(); }

		const Ref<Buffer>& GetPointLightsBuffer() const { return m_LightsManagerTask->GetPointLightsBuffer(); }
		const Ref<Buffer>& GetSpotLightsBuffer() const { return m_LightsManagerTask->GetSpotLightsBuffer(); }
		const Ref<Buffer>& GetDirectionalLightBuffer() const { return m_LightsManagerTask->GetDirectionalLightBuffer(); }

		const auto& GetOpaqueMeshesData() const { return m_GeometryManagerTask->GetOpaqueMeshesData(); }
		const auto& GetTranslucentMeshesData() const { return m_GeometryManagerTask->GetTranslucentMeshesData(); }
		const Ref<Buffer>& GetMeshTransformsBuffer() const { return m_GeometryManagerTask->GetMeshesTransformBuffer(); }
		const Ref<Buffer>& GetMeshPrevTransformsBuffer() const { return m_GeometryManagerTask->GetMeshesPrevTransformBuffer(); }

		const auto& GetOpaqueSpritesData() const { return m_GeometryManagerTask->GetOpaqueSpriteData(); }
		const auto& GetTranslucentSpritesData() const { return m_GeometryManagerTask->GetTranslucentSpriteData(); }
		const Ref<Buffer>& GetSpritesTransformsBuffer() const { return m_GeometryManagerTask->GetSpritesTransformBuffer(); }
		const Ref<Buffer>& GetSpritesPrevTransformBuffer() const { return m_GeometryManagerTask->GetSpritesPrevTransformBuffer(); }

		const LitTextGeometryData& GetOpaqueLitTextData() const { return m_GeometryManagerTask->GetOpaqueLitTextData(); }
		const LitTextGeometryData& GetTranslucentLitTextData() const { return m_GeometryManagerTask->GetTranslucentLitTextData(); }
		const UnlitTextGeometryData& GetUnlitTextData() const { return m_GeometryManagerTask->GetUnlitTextData(); }
		const Ref<Buffer>& GetTextsTransformsBuffer() const { return m_GeometryManagerTask->GetTextsTransformBuffer(); }
		const Ref<Buffer>& GetTextsPrevTransformBuffer() const { return m_GeometryManagerTask->GetTextsPrevTransformBuffer(); }
		const std::vector<Ref<Texture2D>>& GetAtlases() const { return m_GeometryManagerTask->GetAtlases(); }

		const std::vector<Ref<Image>>& GetPointLightShadowMaps() const { return m_ShadowPassTask->GetPointLightShadowMaps(); }
		const std::vector<Ref<Image>>& GetSpotLightShadowMaps() const { return m_ShadowPassTask->GetSpotLightShadowMaps(); }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMaps() const { return m_ShadowPassTask->GetDirectionalLightShadowMaps(); }

		const std::vector<Ref<Sampler>>& GetPointLightShadowMapsSamplers() const { return m_ShadowPassTask->GetPointLightShadowMapsSamplers(); }
		const std::vector<Ref<Sampler>>& GetSpotLightShadowMapsSamplers() const { return m_ShadowPassTask->GetSpotLightShadowMapsSamplers(); }
		const std::vector<Ref<Sampler>>& GetDirectionalLightShadowMapsSamplers() const { return m_ShadowPassTask->GetDirectionalLightShadowMapsSamplers(); }

		// Contains View Matrix and ViewProjInv matrix
		const Ref<Buffer>& GetCameraBuffer() const { return m_PBRPassTask->GetCameraBuffer(); }
		const Ref<Image>& GetSMDistribution() const { return m_PBRPassTask->GetSMDistribution(); }
		const ShaderDefines& GetPBRShaderDefines() const { return m_PBRPassTask->GetPBRShaderDefines(); }

		const Ref<Image>& GetSSAOResult() const { return m_SSAOTask->GetResult(); }
		const Ref<Image>& GetGTAOResult() const { return m_GTAOTask->GetResult(); }
		// ------------------------------------------------

		const GBuffer& GetGBuffer() const { return m_GBuffer; }
		GBuffer& GetGBuffer() { return m_GBuffer; }
		const Ref<Image>& GetOutput() const { return m_FinalImage; }
		const Ref<Image>& GetHDROutput() const { return m_HDRRTImage; }

		const SceneRendererSettings& GetOptions_RT() const { return m_Options_RT; }
		const SceneRendererSettings& GetOptions() const { return m_Options; }
		const glm::mat4& GetViewMatrix() const { return m_View; }
		const glm::mat4& GetProjectionMatrix() const { return m_Projection; }
		const glm::mat4& GetViewProjection() const { return m_ViewProjection; }
		const glm::vec3 GetViewPosition() const { return m_ViewPos; }
		float GetPhotoLinearScale() const { return m_PhotoLinearScale; }

		// Prev frame data
		const glm::mat4& GetPrevViewMatrix() const { return m_PrevView; }
		const glm::mat4& GetPrevProjectionMatrix() const { return m_PrevProjection; }
		const glm::mat4& GetPrevViewProjection() const { return m_PrevViewProjection; }

		const std::vector<glm::mat4>& GetCascadeProjections() const { return m_CameraCascadeProjections; }
		const std::vector<float>& GetCascadeFarPlanes() const { return m_CameraCascadeFarPlanes; }
		float GetShadowMaxDistance() const { return m_MaxShadowDistance; }

	private:
		void InitWithOptions();

	private:
		Scope<GeometryManagerTask> m_GeometryManagerTask;
		Scope<RendererTask> m_RenderMeshesTask;
		Scope<RendererTask> m_RenderSpritesTask;
		Scope<RenderTextLitTask> m_RenderLitTextTask;
		Scope<RenderTextUnlitTask> m_RenderUnlitTextTask;
		Scope<LightsManagerTask> m_LightsManagerTask;
		Scope<RenderLinesTask> m_RenderLinesTask;
		Scope<RenderBillboardsTask> m_RenderBillboardsTask;
		Scope<PBRPassTask> m_PBRPassTask;
		Scope<ShadowPassTask> m_ShadowPassTask;
		Scope<RendererTask> m_BloomTask;
		Scope<RendererTask> m_SkyboxPassTask;
		Scope<RendererTask> m_PostProcessingPassTask;
		Scope<SSAOTask> m_SSAOTask;
		Scope<GTAOTask> m_GTAOTask;
		Scope<RendererTask> m_GridTask;
		Scope<RendererTask> m_TransparencyTask;
		
		GBuffer m_GBuffer;
		Ref<Image> m_FinalImage;
		Ref<Image> m_HDRRTImage; // Render target
		Ref<TextureCube> m_Cubemap;
		glm::mat4 m_View = glm::mat4(1.f);
		glm::mat4 m_Projection = glm::mat4(1.f);
		glm::mat4 m_ViewProjection = glm::mat4(1.f);
		glm::vec3 m_ViewPos = glm::vec3(0.f);

		// Prev frame data
		glm::mat4 m_PrevView = glm::mat4(1.f);
		glm::mat4 m_PrevProjection = glm::mat4(1.f);
		glm::mat4 m_PrevViewProjection = glm::mat4(1.f);

		std::vector<glm::mat4> m_CameraCascadeProjections = std::vector<glm::mat4>(EG_CASCADES_COUNT);
		std::vector<float> m_CameraCascadeFarPlanes = std::vector<float>(EG_CASCADES_COUNT);
		float m_MaxShadowDistance = 1.f;

		glm::uvec2 m_Size = { 1, 1 };
		float m_PhotoLinearScale = 1.f;
		SceneRendererSettings m_Options_RT; // Render thread
		SceneRendererSettings m_Options;

		uint32_t m_FrameIndex = 0;

		bool m_bGridEnabled = false;
	};
}
