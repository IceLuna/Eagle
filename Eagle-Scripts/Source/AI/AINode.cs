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
        // Called once before task is about to run
        public virtual void OnBegin() { }

        // Called once after the task is finished/aborted
        public virtual void OnEnd(AINodeStatus status) {}

        protected void ResetDecorators()
        {
            foreach (var decorator in m_Decorators)
                decorator.Reset();
        }

        protected virtual AINodeStatus Update(float ts) { return AINodeStatus.Succeeded; }

        public AINodeStatus Run(float ts)
        {
            if (m_LastStatus != AINodeStatus.Running)
            {
                OnBegin();
            }
            
            m_LastStatus = Update(ts);
            
            if (m_LastStatus != AINodeStatus.Running)
            {
                OnEnd(m_LastStatus);
                ResetDecorators();
            }

            return m_LastStatus;
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
        protected bool CanRun(float ts, out AINodeStatus status)
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
        private AINodeStatus m_LastStatus = AINodeStatus.Succeeded;
    }
}
