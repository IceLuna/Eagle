#pragma once

#include "GraphNode.h"

namespace Eagle
{
	class AnimationStateMachineGraph;

	class AnimationGraphNode : public GraphNode
	{
	public:
		AnimationGraphNode(const Weak<AnimationGraph>& graph, size_t numInputs);

		const Ref<SkeletalMesh>& GetSkeletal() const { return m_Skeletal; }

	protected:
		// A helper function
		template<typename T, class... Args>
		Ref<T> CloneNode(Args&&... args) const
		{
			Ref<T> clone = GraphNode::CloneNode<T>(std::forward<Args>(args)...);
			clone->m_Skeletal = m_Skeletal;

			return clone;
		}

	protected:
		Ref<SkeletalMesh> m_Skeletal;
	};

	class AnimationGraphNodeOutput : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeOutput(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeOutput>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeStateOutput : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeStateOutput(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeStateOutput>(newGraph);
		}

	private:

		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeTransitionOutput : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeTransitionOutput(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		bool ShouldTransition() const { return m_bTransition; }
		float GetTransitionTime() const { return m_TransitionTime; }
		bool ShouldUseSmoothTransition() const { return m_bUseSmoothTransition; }

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			auto clone = AnimationGraphNode::CloneNode<AnimationGraphNodeTransitionOutput>(newGraph);
			clone->m_TransitionTime = m_TransitionTime;
			clone->m_bTransition = m_bTransition;
			clone->m_bUseSmoothTransition = m_bUseSmoothTransition;
			return clone;
		}

	private:
		float m_TransitionTime = 0.f;
		bool m_bTransition = false;
		bool m_bUseSmoothTransition = true;
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphStateMachineEntry : public AnimationGraphNode
	{
	public:
		AnimationGraphStateMachineEntry(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		void SetStateMachineIndex(uint32_t index) { m_StateMachineIndex = index; }
		const Ref<AnimationStateMachineGraph>& GetStateMachine() const;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override;

	private:
		uint32_t m_StateMachineIndex = ~0u;

		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeClip : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeClip(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			auto clone = AnimationGraphNode::CloneNode<AnimationGraphNodeClip>(newGraph);
			clone->CurrentTime = CurrentTime;
			clone->m_LastAnim = m_LastAnim;
			clone->m_PrevTime = m_PrevTime;
			clone->m_PrevSpeed = m_PrevSpeed;

			return clone;
		}

	public:
		float CurrentTime = 0.f;

	private:
		const void* m_LastAnim = nullptr; // Used to detect the animation clip changes. If it changed, CurrentTime is reset to 0
		float m_PrevTime = 0.f;
		float m_PrevSpeed = 1.f;

		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeBlend : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeBlend(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeBlend>(newGraph);
		}

		const SkeletalPose& Update(Timestep ts) override;

	private:
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeFilterBones : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeFilterBones(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeFilterBones>(newGraph);
		}

		const SkeletalPose& Update(Timestep ts) override;

	private:
		static constexpr size_t s_Inputs = 5;
	};

	class AnimationGraphNodeTransformBone : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeTransformBone(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeTransformBone>(newGraph);
		}

		const SkeletalPose& Update(Timestep ts) override;

	private:
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeAdditiveBlend : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeAdditiveBlend(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeAdditiveBlend>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeCalculateAdditive : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeCalculateAdditive(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeCalculateAdditive>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSelectPoseByBool : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeSelectPoseByBool(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeSelectPoseByBool>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 3;
	};

	class AnimationGraphNodeCachePose : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeCachePose(const Weak<AnimationGraph>& graph) : AnimationGraphNode(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNode::CloneNode<AnimationGraphNodeCachePose>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	// Base class for such nodes as: less, and, etc..
	class AnimationGraphNodeBool : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeBool(const Weak<AnimationGraph>& graph, size_t numInputs) : AnimationGraphNode(graph, numInputs) {}

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
		AnimationGraphNodeAnd(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeAnd>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeOr : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeOr(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeOr>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeXor : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeXor(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeXor>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeNot : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeNot(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeNot>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeLess : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeLess(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeLess>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeLessEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeLessEqual(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeLessEqual>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeGreater : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeGreater(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeGreater>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeGreaterEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeGreaterEqual(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeGreaterEqual>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeEqual(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeEqual>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeNotEqual : public AnimationGraphNodeBool
	{
	public:
		AnimationGraphNodeNotEqual(const Weak<AnimationGraph>& graph) : AnimationGraphNodeBool(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeBool::CloneNode<AnimationGraphNodeNotEqual>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	// Base class for such math nodes as: add, multiply, etc...
	class AnimationGraphNodeFloat : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeFloat(const Weak<AnimationGraph>& graph, size_t numInputs) : AnimationGraphNode(graph, numInputs) {}

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
		AnimationGraphNodeAdd(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeAdd>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSub : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSub(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSub>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeMul : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeMul(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeMul>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeDiv : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeDiv(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeDiv>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 2;
	};

	class AnimationGraphNodeSqrt : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSqrt(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSqrt>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeAbs : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeAbs(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeAbs>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeSin : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeSin(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeSin>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeCos : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeCos(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeCos>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeASin : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeASin(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeASin>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeACos : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeACos(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeACos>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeToRad : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeToRad(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeToRad>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeToDeg : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeToDeg(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeToDeg>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 1;
	};

	class AnimationGraphNodeMapRange : public AnimationGraphNodeFloat
	{
	public:
		AnimationGraphNodeMapRange(const Weak<AnimationGraph>& graph) : AnimationGraphNodeFloat(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeFloat::CloneNode<AnimationGraphNodeMapRange>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 5;
	};

	// Base class for such nodes that return Vec4
	class AnimationGraphNodeVec4 : public AnimationGraphNode
	{
	public:
		AnimationGraphNodeVec4(const Weak<AnimationGraph>& graph, size_t numInputs) : AnimationGraphNode(graph, numInputs) {}

	protected:
		template<typename T, class... Args>
		Ref<T> CloneNode(Args&&... args) const
		{
			Ref<T> clone = AnimationGraphNode::CloneNode<T>(std::forward<Args>(args)...);
			clone->Result = Result;

			return clone;
		}

	public:
		glm::vec4 Result = glm::vec4(0.f);
	};

	class AnimationGraphNodeEulerToQuat : public AnimationGraphNodeVec4
	{
	public:
		AnimationGraphNodeEulerToQuat(const Weak<AnimationGraph>& graph) : AnimationGraphNodeVec4(graph, s_Inputs) {}

		const SkeletalPose& Update(Timestep ts) override;

		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const override
		{
			return AnimationGraphNodeVec4::CloneNode<AnimationGraphNodeEulerToQuat>(newGraph);
		}

	private:
		static constexpr size_t s_Inputs = 5;
	};
}
