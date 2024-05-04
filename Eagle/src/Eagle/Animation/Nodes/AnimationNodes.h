#pragma once

#include "GraphNode.h"

namespace Eagle
{
	class AnimationGraph;
	class AnimationStateMachineGraph;

	class AnimationGraphNode : public GraphNode
	{
	public:
		AnimationGraphNode(const Ref<AnimationGraph>& graph, size_t numInputs)
			: GraphNode(numInputs)
			, m_Graph(graph)
		{}

		const Ref<AnimationGraph>& GetGraph() const { return m_Graph; }

	protected:
		// A helper function
		template<typename T, class... Args>
		Ref<T> CloneNode(Args&&... args) const
		{
			Ref<T> clone = GraphNode::CloneNode<T>(std::forward<Args>(args)...);
			clone->m_Graph = m_Graph;

			return clone;
		}

	protected:
		Ref<AnimationGraph> m_Graph;
	};

	class AnimationGraphNodeOutput : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeOutput(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeOutput>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeStateOutput : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeStateOutput(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeStateOutput>(m_Graph);
		}

	private:

		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeTransitionOutput : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeTransitionOutput(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		bool ShouldTransition() const { return m_bTransition; }
		float GetTransitionTime() const { return m_TransitionTime; }

		Ref<GraphNode> Clone() const override
		{
			auto clone = AnimationGraphNode::CloneNode<AnimationGraphNodeTransitionOutput>(m_Graph);
			clone->m_TransitionTime = m_TransitionTime;
			clone->m_bTransition = m_bTransition;
			return clone;
		}

	private:
		float m_TransitionTime = 0.f;
		bool m_bTransition = false;
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphStateMachineEntry : public AnimationGraphNode
	{
	public:
		AnimationGraphStateMachineEntry(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		void SetStateMachine(const Ref<AnimationStateMachineGraph>& stateMachine) { m_StateMachine = stateMachine; }
		const Ref<AnimationStateMachineGraph>& GetStateMachine() const { return m_StateMachine; }

		void SetVariablesToUse(const VariablesMap& vars);

		Ref<GraphNode> Clone() const override;

	private:
		Ref<AnimationStateMachineGraph> m_StateMachine;

		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeClip : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeClip(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			auto clone = AnimationGraphNode::CloneNode<AnimationGraphNodeClip>(m_Graph);
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

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeBlend>(m_Graph);
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

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeAdditiveBlend>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeCalculateAdditive : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeCalculateAdditive(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeCalculateAdditive>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSelectPoseByBool : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeSelectPoseByBool(const Ref<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeSelectPoseByBool>(m_Graph);
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
		template<typename T, class... Args>
		Ref<T> CloneNode(Args&&... args) const
		{
			Ref<T> clone = AnimationGraphNode::CloneNode<T>(std::forward<Args>(args)...);
			clone->Result = Result;

			return clone;
		}

	public:
		bool Result = false;
	};

	class AnimationGraphNodeAnd : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeAnd(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeAnd>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeOr : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeOr(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeOr>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeXor : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeXor(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeXor>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeNot : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeNot(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeNot>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeLess : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeLess(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeLess>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeLessEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeLessEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeLessEqual>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeGreater : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeGreater(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeGreater>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeGreaterEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeGreaterEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeGreaterEqual>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeEqual>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeNotEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeNotEqual(const Ref<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeNotEqual>(m_Graph);
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
		template<typename T, class... Args>
		Ref<T> CloneNode(Args&&... args) const
		{
			Ref<T> clone = AnimationGraphNode::CloneNode<T>(std::forward<Args>(args)...);
			clone->Result = Result;

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

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeAdd>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSub : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSub(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSub>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeMul : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeMul(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeMul>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeDiv : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeDiv(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeDiv>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSqrt : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSqrt(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSqrt>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeSin : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSin(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSin>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeCos : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeCos(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeCos>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeToRad : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeToRad(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeToRad>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeToDeg : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeToDeg(const Ref<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone() const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeToDeg>(m_Graph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};
}
