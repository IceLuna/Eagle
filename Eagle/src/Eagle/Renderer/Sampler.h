#pragma once

#include "RendererUtils.h"

namespace Eagle
{
	class Sampler
	{
	protected:
		Sampler(FilterMode filterMode, AddressMode addressMode, CompareOperation compareOp, float minLod, float maxLod, float maxAnisotropy = 1.f)
			: m_FilterMode(filterMode)
			, m_AddressMode(addressMode)
			, m_CompareOp(compareOp)
			, m_MinLod(minLod)
			, m_MaxLod(maxLod)
			, m_MaxAnisotropy(maxAnisotropy) {}

	public:
		virtual ~Sampler() = default;

		virtual void* GetHandle() const = 0;

		FilterMode GetFilterMode() const { return m_FilterMode; }
		AddressMode GetAddressMode() const { return m_AddressMode; }
		CompareOperation GetCompareOperation() const { return m_CompareOp; }
		float GetMinLod() const { return m_MinLod; }
		float GetMaxLod() const { return m_MaxLod; }
		float GetMaxAnisotropy() const { return m_MaxAnisotropy; }

		static Ref<Sampler> Create(FilterMode filterMode, AddressMode addressMode, CompareOperation compareOp, float minLod, float maxLod, float maxAnisotropy = 1.f);

	public:
		static Ref<Sampler> PointSampler;
		static Ref<Sampler> TrilinearSampler;

	protected:
		FilterMode m_FilterMode = FilterMode::Point;
		AddressMode m_AddressMode = AddressMode::Wrap;
		CompareOperation m_CompareOp = CompareOperation::Never;
		float m_MinLod = 0.f;
		float m_MaxLod = 0.f;
		float m_MaxAnisotropy = 1.f;
	};

}
