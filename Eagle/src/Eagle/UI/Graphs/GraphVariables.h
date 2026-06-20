#pragma once

#include "Eagle/Core/Core.h"

namespace Eagle
{
	class AssetAnimation;

	enum class GraphVariableType
	{
		Bool = 0,
		Int,
		Float,
		Animation,
		String,
		Vec4,
	};

	using VariablesMap = std::map<std::string, Ref<class GraphVariable>>;

	class GraphVariable
	{
	public:
		// @bDefaultVar. Set to true if this var is created as a default value for the node. This is the case if a user didn't connect anything to the pin
		GraphVariable(GraphVariableType type, bool bDefaultVar = false) : m_Type(type), bDefaultVar(bDefaultVar) {}
		virtual ~GraphVariable() = default;

		GraphVariable(const Ref<GraphVariable>& other)
			: m_Type(other->m_Type), bShowInUI(other->bShowInUI), bDefaultVar(other->bDefaultVar)
		{}

		// Returns false on failure (for example, if variables have different types)
		virtual bool CopyValue(const Ref<GraphVariable>& other) = 0;

		virtual bool HasValue() const = 0;

		GraphVariableType GetType() const { return m_Type; }
		bool IsDefaultVar() const { return bDefaultVar; }

		bool bShowInUI = true;

	private:
		GraphVariableType m_Type;
		bool bDefaultVar = false;
	};

	class GraphVariableBool : public GraphVariable
	{
	public:
		GraphVariableBool(bool val = false, bool bDefaultVar = false) : GraphVariable(GraphVariableType::Bool, bDefaultVar), Value(val) {}

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

	class GraphVariableInt : public GraphVariable
	{
	public:
		GraphVariableInt(int val = 0, bool bDefaultVar = false) : GraphVariable(GraphVariableType::Int, bDefaultVar), Value(val) {}

		GraphVariableInt(const Ref<GraphVariableInt>& other)
			: GraphVariable(other)
			, Value(other->Value)
		{}

		bool CopyValue(const Ref<GraphVariable>& other) override
		{
			EG_CORE_ASSERT(other);
			auto casted = Cast<GraphVariableInt>(other);
			if (!casted)
				return false;

			Value = casted->Value;
			return true;
		}

		virtual bool HasValue() const override { return true; }

		int Value = 0;
	};

	class GraphVariableFloat : public GraphVariable
	{
	public:
		GraphVariableFloat(float val = 0.f, bool bDefaultVar = false) : GraphVariable(GraphVariableType::Float, bDefaultVar), Value(val) {}

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
		GraphVariableAnimation(const Ref<AssetAnimation>& value = nullptr, bool bDefaultVar = false) : GraphVariable(GraphVariableType::Animation, bDefaultVar), Value(value) {}

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
		GraphVariableString(const std::string& val = "", bool bDefaultVar = false) : GraphVariable(GraphVariableType::String, bDefaultVar), Value(val) {}

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
		GraphVariableVec4(const glm::vec4& val = glm::vec4(0), bool bDefaultVar = false) : GraphVariable(GraphVariableType::Vec4, bDefaultVar), Value(val) {}

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
		case GraphVariableType::Int:
			return MakeRef<GraphVariableInt>(Cast<GraphVariableInt>(var));
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
