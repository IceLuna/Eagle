using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Eagle
{
    // Such as sequence, selector
    public class AICompositeNode : AINode
    {
        public T AddChild<T>() where T : AINode, new()
        {
            T child = new T();
            child.SetBlackboard(m_Blackboard);
            m_Children.Add(child);

            return child;
        }

        internal AINode AddChild_Interop(Type type)
        {
            AINode child = Activator.CreateInstance(type) as AINode;
            child.SetBlackboard(m_Blackboard);
            m_Children.Add(child);

            return child;
        }

        public void RemoveChild(AINode child)
        {
            child.SetBlackboard(null);
            m_Children.Remove(child);
        }

        protected override void OnBlackboardChanged()
        {
            base.OnBlackboardChanged();
            foreach (var child in m_Children)
                child.SetBlackboard(m_Blackboard);
        }

        public List<AINode> GetChildren() { return m_Children; }

        protected List<AINode> m_Children = new List<AINode>();
    }

    [UIName("Sequence")]
    [Tooltip("Sequence succeeds when all children succeed, fails on first failure")]
    public class AISequenceNode : AICompositeNode
    {
        protected override AINodeStatus Update(float ts)
        {
            // Doesn't matter what we set here but Running
            m_LastChildStatus = AINodeStatus.Failed;

            if (m_Children.Count == 0)
                return AINodeStatus.Succeeded;

            if (m_Current >=  m_Children.Count)
                return AINodeStatus.Succeeded;

            m_LastChildStatus = m_Children[m_Current].Run(ts);
            switch (m_LastChildStatus)
            {
                case AINodeStatus.Failed:
                {
                    return AINodeStatus.Failed;
                }
                case AINodeStatus.Running:
                {
                    return AINodeStatus.Running;
                }
                case AINodeStatus.Succeeded:
                {
                    ++m_Current;
                    return m_Current < m_Children.Count ? AINodeStatus.Running : AINodeStatus.Succeeded;
                }
            }

            Log.Error("Unhandled AITask status");
            return AINodeStatus.Failed;
        }

        public override void OnEnd(AINodeStatus status)
        {
            base.OnEnd(status);
            // If child was running, and this composite node was terminated, we should call `OnEnd` for the active child
            if (m_LastChildStatus == AINodeStatus.Running)
            {
                m_Children[m_Current].OnEnd(status);
                m_LastChildStatus = status;
            }
            m_Current = 0;
        }

        private int m_Current = 0;
        private AINodeStatus m_LastChildStatus = AINodeStatus.Failed;
    }

    [UIName("Selector")]
    [Tooltip("Selector succeeds on first child success, fails if all fail")]
    public class AISelectorNode : AICompositeNode
    {
        protected override AINodeStatus Update(float ts)
        {
            // Doesn't matter what we set here but Running
            m_LastChildStatus = AINodeStatus.Failed;

            if (m_Children.Count == 0)
                return AINodeStatus.Succeeded;

            if (m_Current >= m_Children.Count)
                return AINodeStatus.Succeeded;

            m_LastChildStatus = m_Children[m_Current].Run(ts);
            switch (m_LastChildStatus)
            {
                case AINodeStatus.Failed:
                {
                    ++m_Current;
                    return m_Current >= m_Children.Count ? AINodeStatus.Failed : AINodeStatus.Running;
                }
                case AINodeStatus.Running:
                {
                    return AINodeStatus.Running;
                }
                case AINodeStatus.Succeeded:
                {
                    return AINodeStatus.Succeeded;
                }
            }

            Log.Error("Unhandled AITask status");
            return AINodeStatus.Failed;
        }

        public override void OnEnd(AINodeStatus status)
        {
            base.OnEnd(status);
            // If child was running, and this composite node was terminated, we should call `OnEnd` for the active child
            if (m_LastChildStatus == AINodeStatus.Running)
            {
                m_Children[m_Current].OnEnd(status);
                m_LastChildStatus = status;
            }
            m_Current = 0;
        }

        private int m_Current = 0;
        private AINodeStatus m_LastChildStatus = AINodeStatus.Failed;
    }
}
