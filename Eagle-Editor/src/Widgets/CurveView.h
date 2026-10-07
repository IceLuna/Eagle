#pragma once

#include "Eagle/Curves/Curve.h"
#include "Eagle/UI/UI.h"

#include <imgui.h>

#include <any>
#include <string>

namespace Eagle
{
	// Type-erased view over a single `Curve<T>`.
	// Supporting a new value type means specializing `CurveAccess` and `CurveUI::DrawValue`
	class CurveView
	{
	public:
		CurveView(const std::string& name, ImU32 color) : m_Name(name), m_Color(color) {}
		virtual ~CurveView() = default;

		const std::string& GetName() const { return m_Name; }
		ImU32 GetColor() const { return m_Color; }

		virtual size_t GetKeysCount() const = 0;
		virtual GUID GetKeyID(size_t index) const = 0;
		virtual float GetKeyTime(size_t index) const = 0;
		virtual CurveInterpolation GetKeyInterpolation(size_t index) const = 0;
		virtual bool FindKeyIndex(const GUID& id, size_t* outIndex) const = 0;

		// Doesn't re-sort, call `Sort()` once all times are updated
		virtual void SetKeyTime(const GUID& id, float time) = 0;
		virtual void Sort() = 0;

		// Switching to Cubic seeds the tangents from the current curve so nothing jumps
		virtual void SetKeyInterpolation(const GUID& id, CurveInterpolation interpolation) = 0;

		// Sets both tangents of a Cubic key to zero
		virtual void FlattenTangents(const GUID& id) = 0;

		virtual bool RemoveKey(const GUID& id) = 0;

		// Adds (or overwrites) a key at `time` holding the channel's current value there.
		// Returns the key's ID.
		virtual GUID AddKeyAtTime(float time) = 0;

		virtual std::any CopyKey(const GUID& id) const = 0;
		// Returns a null GUID when `data` holds a key of a different value type
		virtual GUID PasteKey(const std::any& data, float time) = 0;

		// Returns true if changed
		virtual bool DrawKeyValue(const GUID& id) = 0;

		// Text drawn next to the key in the timeline
		virtual std::string GetKeyLabel(size_t index) const { return {}; }

		// False for channels whose keys never blend (for example, camera cuts).
		// UI then hides interpolation options
		virtual bool IsInterpolationEditable() const { return true; }

		// Channels that can be switched off individually (for example, post process properties).
		// A disabled channel contributes nothing, as if it weren't on the track at all
		virtual bool CanBeDisabled() const { return false; }
		virtual bool IsEnabled() const { return true; }
		virtual void SetEnabled(bool) {}

		// Number of scalar curves this channel can be shown as. 0 hides it from the curve editor
		virtual uint32_t GetCurveComponentsCount() const = 0;
		virtual const char* GetCurveComponentName(uint32_t component) const = 0;
		virtual float EvaluateComponent(float time, uint32_t component) const = 0;
		virtual float GetKeyComponent(size_t index, uint32_t component) const = 0;
		virtual void SetKeyComponent(const GUID& id, uint32_t component, float value) = 0;

		// Effective tangent (units per second) as used by evaluation, whatever the key's mode
		virtual float GetKeyTangent(size_t index, uint32_t component, bool bOut) const = 0;
		// Converts the key to Cubic if needed
		virtual void SetKeyTangent(const GUID& id, uint32_t component, bool bOut, float value) = 0;

		// Two keys closer than this are considered to sit at the same time
		static constexpr float s_KeyTimeTolerance = 1e-4f;

	protected:
		std::string m_Name;
		ImU32 m_Color;
	};

	// Scalar access used by the curve editor. Specialize to expose a new value type as curves
	template <typename T>
	struct CurveAccess;

	// Step values aren't curves
	template <>
	struct CurveAccess<bool>
	{
		static constexpr uint32_t Count = 0;
		static float Get(const bool&, uint32_t) { return 0.f; }
		static void Set(bool&, uint32_t, float) {}
		static const char* Name(uint32_t) { return ""; }
	};

	template <>
	struct CurveAccess<int32_t>
	{
		static constexpr uint32_t Count = 0;
		static float Get(const int32_t&, uint32_t) { return 0.f; }
		static void Set(int32_t&, uint32_t, float) {}
		static const char* Name(uint32_t) { return ""; }
	};

	template <>
	struct CurveAccess<uint32_t>
	{
		static constexpr uint32_t Count = 0;
		static float Get(const uint32_t&, uint32_t) { return 0.f; }
		static void Set(uint32_t&, uint32_t, float) {}
		static const char* Name(uint32_t) { return ""; }
	};

	template <>
	struct CurveAccess<float>
	{
		static constexpr uint32_t Count = 1;
		static float Get(const float& value, uint32_t) { return value; }
		static void Set(float& value, uint32_t, float x) { value = x; }
		static const char* Name(uint32_t) { return "Value"; }
	};

	template <>
	struct CurveAccess<glm::vec2>
	{
		static constexpr uint32_t Count = 2;
		static float Get(const glm::vec2& value, uint32_t c) { return value[c]; }
		static void Set(glm::vec2& value, uint32_t c, float x) { value[c] = x; }
		static const char* Name(uint32_t c)
		{
			static const char* names[] = { "X", "Y" };
			return names[c < 2 ? c : 0];
		}
	};

	template <>
	struct CurveAccess<glm::vec3>
	{
		static constexpr uint32_t Count = 3;
		static float Get(const glm::vec3& value, uint32_t c) { return value[c]; }
		static void Set(glm::vec3& value, uint32_t c, float x) { value[c] = x; }
		static const char* Name(uint32_t c)
		{
			static const char* names[] = { "X", "Y", "Z" };
			return names[c < 3 ? c : 0];
		}
	};

	template <>
	struct CurveAccess<glm::vec4>
	{
		static constexpr uint32_t Count = 4;
		static float Get(const glm::vec4& value, uint32_t c) { return value[c]; }
		static void Set(glm::vec4& value, uint32_t c, float x) { value[c] = x; }
		static const char* Name(uint32_t c)
		{
			static const char* names[] = { "R", "G", "B", "A" };
			return names[c < 4 ? c : 0];
		}
	};

	// Rotations aren't shown as curves: per-component quaternion curves are meaningless to
	// edit by hand, and euler curves would lie about what slerp actually does in between
	template <>
	struct CurveAccess<glm::quat>
	{
		static constexpr uint32_t Count = 0;
		static float Get(const glm::quat&, uint32_t) { return 0.f; }
		static void Set(glm::quat&, uint32_t, float) {}
		static const char* Name(uint32_t) { return ""; }
	};

	template <>
	struct CurveAccess<GUID>
	{
		static constexpr uint32_t Count = 0;
		static float Get(const GUID&, uint32_t) { return 0.f; }
		static void Set(GUID&, uint32_t, float) {}
		static const char* Name(uint32_t) { return ""; }
	};

	template <>
	struct CurveAccess<std::string>
	{
		static constexpr uint32_t Count = 0;
		static float Get(const std::string&, uint32_t) { return 0.f; }
		static void Set(std::string&, uint32_t, float) {}
		static const char* Name(uint32_t) { return ""; }
	};

	namespace CurveUI
	{
		inline bool DrawValue(const GUID&, const char* label, bool& value)
		{
			return UI::Property(label, value);
		}

		inline bool DrawValue(const GUID&, const char* label, int32_t& value)
		{
			return UI::PropertyDrag(label, value, 1.f);
		}

		inline bool DrawValue(const GUID&, const char* label, uint32_t& value)
		{
			return UI::PropertyDrag(label, value, 1.f);
		}

		inline bool DrawValue(const GUID&, const char* label, float& value)
		{
			return UI::PropertyDrag(label, value, 0.05f);
		}

		inline bool DrawValue(const GUID&, const char* label, glm::vec2& value)
		{
			return UI::PropertyDrag(label, value, 0.05f);
		}

		inline bool DrawValue(const GUID&, const char* label, glm::vec3& value)
		{
			return UI::PropertyDrag(label, value, 0.05f);
		}

		inline bool DrawValue(const GUID&, const char* label, glm::vec4& value)
		{
			return UI::PropertyColor(label, value);
		}

		inline bool DrawValue(const GUID&, const char* label, GUID& value)
		{
			UI::Text(label, std::to_string(value.GetHash()));
			return false;
		}

		inline bool DrawValue(const GUID&, const char* label, std::string& value)
		{
			return UI::PropertyText(label, value);
		}

		inline bool DrawValue(const GUID&, const char* label, glm::quat& value)
		{
			ImGui::Columns(1);
			const bool bChanged = UI::DrawQuatControl(std::string(label) + " (Quat)", value);
			ImGui::Columns(2);
			return bChanged;
		}
	}

	// Concrete view over a `Curve<T>`. One template covers every value type; the per-type bits live in
	// `CurveValueTraits`, `CurveAccess` and the `CurveUI::DrawValue` overloads
	template <typename T>
	class TypedCurveView : public CurveView
	{
	public:
		using Access = CurveAccess<T>;
		using Traits = CurveValueTraits<T>;

		TypedCurveView(const std::string& name, ImU32 color, Curve<T>* channel, const T& defaultValue)
			: CurveView(name, color), m_Curve(channel), m_DefaultValue(defaultValue) {}

		size_t GetKeysCount() const override { return m_Curve->GetKeysCount(); }
		GUID GetKeyID(size_t index) const override { return m_Curve->GetKey(index).ID; }
		float GetKeyTime(size_t index) const override { return m_Curve->GetKey(index).Time; }
		CurveInterpolation GetKeyInterpolation(size_t index) const override { return m_Curve->GetKey(index).Interpolation; }
		bool FindKeyIndex(const GUID& id, size_t* outIndex) const override { return m_Curve->GetKeyIndex(id, outIndex); }

		void SetKeyTime(const GUID& id, float time) override
		{
			if (auto* key = m_Curve->FindKey(id))
				key->Time = glm::max(0.f, time);
		}

		void Sort() override { m_Curve->SortKeys(); }

		void SetKeyInterpolation(const GUID& id, CurveInterpolation interpolation) override
		{
			size_t index = 0;
			if (!m_Curve->GetKeyIndex(id, &index))
				return;

			auto& key = m_Curve->GetKey(index);
			if (key.Interpolation == interpolation)
				return;

			if (interpolation == CurveInterpolation::Cubic)
			{
				// Seed the tangents from the curve as it is right now, so converting a key to
				// Cubic doesn't visibly change anything until the user starts editing the handles
				const T inTangent = m_Curve->GetInTangent(index);
				const T outTangent = m_Curve->GetOutTangent(index);
				key.InTangent = inTangent;
				key.OutTangent = outTangent;
			}
			key.Interpolation = interpolation;
		}

		void FlattenTangents(const GUID& id) override
		{
			auto* key = m_Curve->FindKey(id);
			if (key && key->Interpolation == CurveInterpolation::Cubic)
			{
				key->InTangent = Traits::Zero();
				key->OutTangent = Traits::Zero();
			}
		}

		bool RemoveKey(const GUID& id) override { return m_Curve->RemoveKey(id); }

		GUID AddKeyAtTime(float time) override
		{
			time = glm::max(0.f, time);
			const T value = m_Curve->Evaluate(time, m_DefaultValue);

			// Inherit the mode of the key to the left, so inserting a key mid-curve doesn't
			// change the character of the segment it lands in
			CurveInterpolation interpolation = CurveInterpolation::Smooth;
			for (const auto& key : m_Curve->GetKeys())
			{
				if (key.Time <= time)
					interpolation = key.Interpolation;
				else
					break;
			}

			const size_t index = m_Curve->AddKey(time, value, interpolation);
			return m_Curve->GetKey(index).ID;
		}

		std::any CopyKey(const GUID& id) const override
		{
			if (const auto* key = m_Curve->FindKey(id))
				return std::any(*key);
			return {};
		}

		GUID PasteKey(const std::any& data, float time) override
		{
			const CurveKey<T>* source = std::any_cast<CurveKey<T>>(&data);
			if (!source)
				return GUID(0, 0);

			CurveKey<T> pasted = *source;
			pasted.ID = GUID();
			pasted.Time = glm::max(0.f, time);

			// Pasting onto an existing key replaces it rather than stacking two keys at one time
			if (auto* existing = m_Curve->FindKeyAtTime(pasted.Time, s_KeyTimeTolerance))
			{
				const GUID existingID = existing->ID;
				*existing = pasted;
				existing->ID = existingID;
				return existingID;
			}

			const size_t index = m_Curve->InsertKey(pasted);
			return m_Curve->GetKey(index).ID;
		}

		bool DrawKeyValue(const GUID& id) override
		{
			auto* key = m_Curve->FindKey(id);
			if (!key)
				return false;

			bool bChanged = CurveUI::DrawValue(id, "Value", key->Value);

			if constexpr (Access::Count > 0)
			{
				if (key->Interpolation == CurveInterpolation::Cubic)
				{
					bChanged |= CurveUI::DrawValue(id, "In Tangent", key->InTangent);
					bChanged |= CurveUI::DrawValue(id, "Out Tangent", key->OutTangent);
				}
			}
			return bChanged;
		}

		uint32_t GetCurveComponentsCount() const override { return Access::Count; }
		const char* GetCurveComponentName(uint32_t component) const override { return Access::Name(component); }

		float EvaluateComponent(float time, uint32_t component) const override
		{
			return Access::Get(m_Curve->Evaluate(time, m_DefaultValue), component);
		}

		float GetKeyComponent(size_t index, uint32_t component) const override
		{
			return Access::Get(m_Curve->GetKey(index).Value, component);
		}

		void SetKeyComponent(const GUID& id, uint32_t component, float value) override
		{
			if (auto* key = m_Curve->FindKey(id))
				Access::Set(key->Value, component, value);
		}

		float GetKeyTangent(size_t index, uint32_t component, bool bOut) const override
		{
			return Access::Get(bOut ? m_Curve->GetOutTangent(index) : m_Curve->GetInTangent(index), component);
		}

		void SetKeyTangent(const GUID& id, uint32_t component, bool bOut, float value) override
		{
			size_t index = 0;
			if (!m_Curve->GetKeyIndex(id, &index))
				return;

			SetKeyInterpolation(id, CurveInterpolation::Cubic);
			auto& key = m_Curve->GetKey(index);
			Access::Set(bOut ? key.OutTangent : key.InTangent, component, value);
		}

	protected:
		Curve<T>* m_Curve = nullptr; // Owned by the track
		T m_DefaultValue;
	};

	// A bool curve plotted as an on/off (1/0) step curve. `CurveAccess<bool>` deliberately hides bools from the curve editor
	// (the scene sequence editor relies on that), so editors that want to plot them use this view instead.
	// Dragged keys snap to on (above 0.5) or off
	class OnOffCurveView : public TypedCurveView<bool>
	{
	public:
		using TypedCurveView<bool>::TypedCurveView;

		bool IsInterpolationEditable() const override { return false; }

		uint32_t GetCurveComponentsCount() const override { return 1u; }
		const char* GetCurveComponentName(uint32_t) const override { return "On"; }
		float EvaluateComponent(float time, uint32_t) const override { return m_Curve->Evaluate(time, m_DefaultValue) ? 1.f : 0.f; }
		float GetKeyComponent(size_t index, uint32_t) const override { return m_Curve->GetKey(index).Value ? 1.f : 0.f; }
		void SetKeyComponent(const GUID& id, uint32_t, float value) override
		{
			if (auto* key = m_Curve->FindKey(id))
				key->Value = value >= 0.5f;
		}

		float GetKeyTangent(size_t, uint32_t, bool) const override { return 0.f; }
		void SetKeyTangent(const GUID&, uint32_t, bool, float) override {}
	};
}
