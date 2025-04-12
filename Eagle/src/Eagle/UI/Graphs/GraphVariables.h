#pragma once

#include "Eagle/Core/Core.h"

namespace Eagle
{
	class AssetAnimation;

	enum class GraphVariableType
	{
		Bool = 0,
		Float,
		Animation,
		String,
		Vec4,
	};

	using VariablesMap = std::map<std::string, Ref<class GraphVariable>>;

	class GraphVariable
	{
	public:
		GraphVariable(GraphVariableType type) : m_Type(type) {}
		virtual ~GraphVariable() = default;

		GraphVariable(const Ref<GraphVariable>& other)
			: m_Type(other->m_Type), bShowInUI(other->bShowInUI)
		{}

		// Returns false on failure (for example, if variables have different types)
		virtual bool CopyValue(const Ref<GraphVariable>& other) = 0;

		virtual bool HasValue() const = 0;

		GraphVariableType GetType() const { return m_Type; }

		bool bShowInUI = true;

	private:
		GraphVariableType m_Type;
	};

	class GraphVariableBool : public GraphVariable
	{
	public:
		GraphVariableBool(bool val = false) : GraphVariable(GraphVariableType::Bool), Value(val) {}

		GraphVariableBool(const Ref<GraphVariableBool>& other)
			: GraphVariable(other)
			, Value(other->Value)
		{}

		bool CopyValue(const Ref<GraphVariable>& other) override
		{
			EG_CORE_ASSERT(other);
			auto casted = Cast<GraphVariableBool>(other);
			if (!casted)
				return false;

			Value = casted->Value;
			return true;
		}

		virtual bool HasValue() const override { return true; }

		bool Value = false;
	};

	class GraphVariableFloat : public GraphVariable
	{
	public:
		GraphVariableFloat(float val = 0.f) : GraphVariable(GraphVariableType::Float), Value(val) {}

		GraphVariableFloat(const Ref<GraphVariableFloat>& other)
			: GraphVariable(other)
			, Value(other->Value)
		{}

		bool CopyValue(const Ref<GraphVariable>& other) override
		{
			EG_CORE_ASSERT(other);
			auto casted = Cast<GraphVariableFloat>(other);
			if (!casted)
				return false;

			Value = casted->Value;
			return true;
		}

		virtual bool HasValue() const override { return true; }

		float Value = 0.f;
	};

	class GraphVariableAnimation : public GraphVariable
	{
	public:
		GraphVariableAnimation(const Ref<AssetAnimation>& value = nullptr) : GraphVariable(GraphVariableType::Animation), Value(value) {}

		GraphVariableAnimation(const Ref<GraphVariableAnimation>& other)
			: GraphVariable(other)
			, Value(other->Value)
		{}

		bool CopyValue(const Ref<GraphVariable>& other) override
		{
			EG_CORE_ASSERT(other);
			auto casted = Cast<GraphVariableAnimation>(other);
			if (!casted)
				return false;

			Value = casted->Value;
			return true;
		}

		virtual bool HasValue() const override { return Value.operator bool(); }

		Ref<AssetAnimation> Value;
	};

	class GraphVariableString : public GraphVariable
	{
	public:
		GraphVariableString(const std::string& val = "") : GraphVariable(GraphVariableType::String), Value(val) {}

		GraphVariableString(const Ref<GraphVariableString>& other)
			: GraphVariable(other)
			, Value(other->Value)
		{}

		bool CopyValue(const Ref<GraphVariable>& other) override
		{
			EG_CORE_ASSERT(other);
			auto casted = Cast<GraphVariableString>(other);
			if (!casted)
				return false;

			Value = casted->Value;
			return true;
		}

		virtual bool HasValue() const override { return true; }

		std::string Value;
	};

	class GraphVariableVec4 : public GraphVariable
	{
	public:
		GraphVariableVec4(const glm::vec4& val = glm::vec4(0)) : GraphVariable(GraphVariableType::Vec4), Value(val) {}

		GraphVariableVec4(const Ref<GraphVariableVec4>& other)
			: GraphVariable(other)
			, Value(other->Value)
		{}

		bool CopyValue(const Ref<GraphVariable>& other) override
		{
			EG_CORE_ASSERT(other);
			auto casted = Cast<GraphVariableVec4>(other);
			if (!casted)
				return false;

			Value = casted->Value;
			return true;
		}

		virtual bool HasValue() const override { return true; }

		glm::vec4 Value = glm::vec4(0);
	};

	static Ref<GraphVariable> CopyVarByType(const Ref<GraphVariable>& var)
	{
		if (!var)
			return nullptr;

		switch (var->GetType())
		{
		case GraphVariableType::Bool:
			return MakeRef<GraphVariableBool>(Cast<GraphVariableBool>(var));
		case GraphVariableType::Float:
			return MakeRef<GraphVariableFloat>(Cast<GraphVariableFloat>(var));
		case GraphVariableType::Animation:
			return MakeRef<GraphVariableAnimation>(Cast<GraphVariableAnimation>(var));
		case GraphVariableType::String:
			return MakeRef<GraphVariableString>(Cast<GraphVariableString>(var));
		case GraphVariableType::Vec4:
			return MakeRef<GraphVariableVec4>(Cast<GraphVariableVec4>(var));
		default:
			EG_CORE_ASSERT(false);
			return {};
		}
	}
}
