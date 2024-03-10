#pragma once

#include "Animation.h"
#include "AnimationGraphVariables.h"
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	class AnimationGraph;
	struct SkeletalPose;

	class AnimationGraphNode
	{
	public:
		AnimationGraphNode(const Ref<AnimationGraph>& graph, size_t numInputs) : m_Graph(graph)
		{
			m_Inputs.resize(numInputs);
			m_Variables.resize(numInputs);
		}

		virtual ~AnimationGraphNode() = default;

		virtual const SkeletalPose& Update(Timestep ts) = 0;
		virtual Ref<AnimationGraphNode> Clone() const = 0;

		void SetInput(const Ref<AnimationGraphNode>& node, size_t index)
		{
			m_Inputs[index] = node;
			m_Variables[index].reset();
		}

		void SetInput(const Ref<AnimationGraphVariable>& var, size_t index)
		{
			m_Variables[index] = var;
			m_Inputs[index].reset();
		}

		const std::vector<Ref<AnimationGraphNode>>& GetInputNodes() const { return m_Inputs; }
		const std::vector<Ref<AnimationGraphVariable>>& GetInputVariables() const { return m_Variables; }

		void Reset()
		{
			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				m_Inputs[i].reset();
				m_Variables[i].reset();
			}
		}

		const Ref<AnimationGraph>& GetGraph() const { return m_Graph; }
		const SkeletalPose& GetPose() const { return m_Pose; }

	protected:		
		template<typename T>
		Ref<T> CloneNode() const
		{
			Ref<T> clone = MakeRef<T>(m_Graph);
			clone->m_Pose = m_Pose;
			clone->m_CalculatedOnFrame = m_CalculatedOnFrame;

			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				clone->m_Inputs[i] = m_Inputs[i] ? m_Inputs[i]->Clone() : nullptr;
				clone->m_Variables[i] = m_Variables[i]; // Vars are copied as is
			}

			return clone;
		}

	protected:
		Ref<AnimationGraph> m_Graph;

		std::vector<Ref<AnimationGraphNode>> m_Inputs;
		std::vector<Ref<AnimationGraphVariable>> m_Variables;
		SkeletalPose m_Pose; // Pose that was calculated by the node during the latest update
		size_t m_CalculatedOnFrame = 0;
	};

	class AnimationGraphNodeClip : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeClip(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;
		
		Ref<AnimationGraphNode> Clone() const override
		{
			auto clone = AnimationGraphNode::CloneNode<AnimationGraphNodeClip>();
			clone->CurrentTime = CurrentTime;
			clone->m_LastAnim = m_LastAnim;

			return clone;
		}

	public:
		float CurrentTime = 0.f;

	private:
		const void* m_LastAnim = nullptr; // Used to detect the animation clip changes. If it changed, CurrentTime is reset to 0
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeBlend : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeBlend(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeBlend>();
		}

		const SkeletalPose& Update(Timestep ts) override;

	private:
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeAdditiveBlend : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeAdditiveBlend(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeAdditiveBlend>();
		}

	private:
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeCalculateAdditive : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeCalculateAdditive(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeCalculateAdditive>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSelectPoseByBool : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeSelectPoseByBool(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeSelectPoseByBool>();
		}

	private:
		static constexpr size_t s_Inputs = 3;
	};

	// Base class for such nodes as: less, and, etc..
	class AnimationGraphNodeBool : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeBool(const Ref<AnimationGraph>& graph, size_t numInputs) : AnimationGraphNode(graph, numInputs) {}

	protected:
		template<typename T>
		Ref<T> CloneNode() const
		{
			Ref<T> clone = MakeRef<T>(m_Graph);
			clone->m_Pose = m_Pose;
			clone->m_CalculatedOnFrame = m_CalculatedOnFrame;
			clone->bResult = bResult;

			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				clone->m_Inputs[i] = m_Inputs[i] ? m_Inputs[i]->Clone() : nullptr;
				clone->m_Variables[i] = m_Variables[i]; // Vars are copied as is
			}

			return clone;
		}

	public:
		bool bResult = false;
	};

	class AnimationGraphNodeAnd : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeAnd(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeAnd>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeOr : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeOr(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeOr>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeXor : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeXor(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeXor>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeNot : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeNot(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeNot>();
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeLess : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeLess(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeLess>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeLessEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeLessEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeLessEqual>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeGreater : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeGreater(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeGreater>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeGreaterEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeGreaterEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeGreaterEqual>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeEqual>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeNotEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeNotEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeNotEqual>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	// Base class for such math nodes as: add, multiply, etc...
	class AnimationGraphNodeFloat : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeFloat(const Ref<AnimationGraph>& graph, size_t numInputs) : AnimationGraphNode(graph, numInputs) {}

	protected:
		template<typename T>
		Ref<T> CloneNode() const
		{
			Ref<T> clone = MakeRef<T>(m_Graph);
			clone->m_Pose = m_Pose;
			clone->m_CalculatedOnFrame = m_CalculatedOnFrame;
			clone->Result = Result;

			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				clone->m_Inputs[i] = m_Inputs[i] ? m_Inputs[i]->Clone() : nullptr;
				clone->m_Variables[i] = m_Variables[i]; // Vars are copied as is
			}

			return clone;
		}

	public:
		float Result = 0.f;
	};

	class AnimationGraphNodeAdd : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeAdd(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeAdd>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSub : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSub(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSub>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeMul : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeMul(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeMul>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeDiv : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeDiv(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeDiv>();
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSin : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSin(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSin>();
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeCos : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeCos(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeCos>();
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeToRad : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeToRad(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeToRad>();
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeToDeg : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeToDeg(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<AnimationGraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeToDeg>();
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};
}
