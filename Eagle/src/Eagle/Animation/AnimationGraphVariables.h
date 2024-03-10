#pragma once

namespace Eagle
{
	class AssetAnimation;

	enum class GraphVariableType
	{
		Bool = 0,
		Float,
		Animation
	};

	class AnimationGraphVariable
	{
	public:
		AnimationGraphVariable(GraphVariableType type) : m_Type(type) {}
		virtual ~AnimationGraphVariable() = default;

		AnimationGraphVariable(const Ref<AnimationGraphVariable>& other)
		{
			m_Type = other->m_Type;
		}

		virtual bool HasValue() const = 0;

		GraphVariableType GetType() const { return m_Type; }

	private:
		GraphVariableType m_Type;
	};

	class AnimationGraphVariableBool : public AnimationGraphVariable
	{
	public:
		AnimationGraphVariableBool(bool val = false) : AnimationGraphVariable(GraphVariableType::Bool), Value(val) {}

		AnimationGraphVariableBool(const Ref<AnimationGraphVariableBool>& other)
			: AnimationGraphVariable(other)
		{
			Value = other->Value;
		}

		virtual bool HasValue() const override { return true; }

		bool Value = false;
	};

	class AnimationGraphVariableFloat : public AnimationGraphVariable
	{
	public:
		AnimationGraphVariableFloat(float val = 0.f) : AnimationGraphVariable(GraphVariableType::Float), Value(val) {}

		AnimationGraphVariableFloat(const Ref<AnimationGraphVariableFloat>& other)
			: AnimationGraphVariable(other)
		{
			Value = other->Value;
		}

		virtual bool HasValue() const override { return true; }

		float Value = 0.f;
	};

	class AnimationGraphVariableAnimation : public AnimationGraphVariable
	{
	public:
		AnimationGraphVariableAnimation(const Ref<AssetAnimation>& value = nullptr) : AnimationGraphVariable(GraphVariableType::Animation), Value(value) {}

		AnimationGraphVariableAnimation(const Ref<AnimationGraphVariableAnimation>& other)
			: AnimationGraphVariable(other)
		{
			Value = other->Value;
		}

		virtual bool HasValue() const override { return Value.operator bool(); }

		Ref<AssetAnimation> Value;
	};

	static Ref<AnimationGraphVariable> CopyVarByType(const Ref<AnimationGraphVariable>& var)
	{
		if (!var)
			return nullptr;

		switch (var->GetType())
		{
		case GraphVariableType::Bool:
			return MakeRef<AnimationGraphVariableBool>(Cast<AnimationGraphVariableBool>(var));
		case GraphVariableType::Float:
			return MakeRef<AnimationGraphVariableFloat>(Cast<AnimationGraphVariableFloat>(var));
		case GraphVariableType::Animation:
			return MakeRef<AnimationGraphVariableAnimation>(Cast<AnimationGraphVariableAnimation>(var));
		default:
			EG_CORE_ASSERT(false);
			return {};
		}
	}
}
