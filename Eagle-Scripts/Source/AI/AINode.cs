using System;
using System.Collections.Generic;
using System.Threading;

namespace Eagle
{
    public enum AINodeStatus
    {
        Failed,
        Running,
        Succeeded,
    }

    public class AINode
    {
        // Called once before the node's first `Update`, after the decorators let it run
        public virtual void OnBegin() { }

        // Called once after the node is finished/aborted
        public virtual void OnEnd(AINodeStatus status) {}

        protected void ResetDecorators()
        {
            foreach (var decorator in m_Decorators)
                decorator.Reset();
        }

        protected virtual AINodeStatus Update(float ts) { return AINodeStatus.Succeeded; }

        public AINodeStatus Run(float ts)
        {
            if (CanRun(ts, out AINodeStatus status))
            {
                if (!m_bBegun)
                {
                    m_bBegun = true;
                    OnBegin();
                }

                status = Update(ts);
            }

            if (status != AINodeStatus.Running)
                End(status);

            return status;
        }

        // Composite nodes call it on their running child when they're aborted, so that the child starts over the next time it's run
        internal void End(AINodeStatus status)
        {
            bool bBegun = m_bBegun;
            m_bBegun = false;

            if (bBegun)
                OnEnd(status);

            foreach (var decorator in m_Decorators)
                decorator.OnNodeEnd(status, bBegun);

            ResetDecorators();
        }

        public T AddDecorator<T>() where T : AINodeDecorator, new()
        {
            T decorator = new T();
            decorator.SetBlackboard(m_Blackboard);
            m_Decorators.Add(decorator);
            return decorator;
        }

        internal AINodeDecorator AddDecorator_Interop(Type type)
        {
            AINodeDecorator decorator = Activator.CreateInstance(type) as AINodeDecorator;
            decorator.SetBlackboard(m_Blackboard);
            m_Decorators.Add(decorator);
            return decorator;
        }

        public bool RemoveDecorator(AINodeDecorator decorator)
        {
            return m_Decorators.Remove(decorator);
        }

        internal void SetBlackboard(AIBlackboard blackboard)
        {
            m_Blackboard = blackboard;
            foreach (var decorator in m_Decorators)
                decorator.SetBlackboard(blackboard);

            OnBlackboardChanged();
        }

        protected virtual void OnBlackboardChanged() {}

        // Checks decorators
        private bool CanRun(float ts, out AINodeStatus status)
        {
            status = AINodeStatus.Succeeded;
            foreach (var decorator in m_Decorators)
            {
                status = decorator.Update(ts);
                if (status != AINodeStatus.Succeeded)
                    return false;
            }

            return true;
        }

        protected List<AINodeDecorator> m_Decorators = new List<AINodeDecorator>();
        protected AIBlackboard m_Blackboard = null;
        private bool m_bBegun = false;
    }
}
