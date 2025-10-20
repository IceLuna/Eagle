using System;

namespace Eagle
{
    // Allows to run checks before/when a node is running
    public class AINodeDecorator
    {
        // The task should continue execution only if `Succeeded` is returned.
        // The task should fail if `Failed` is returned.
        // The task should skip its execution and wait if `Running` is returned.
        public virtual AINodeStatus Update(float ts) { return AINodeStatus.Failed; }

        public virtual void Reset() { } // Resets the state
        
        internal void SetBlackboard(AIBlackboard blackboard)
        {
            m_Blackboard = blackboard;
            OnBlackboardChanged();
        }

        protected virtual void OnBlackboardChanged() { }

        protected AIBlackboard m_Blackboard = null;
    }

    [UIName("Delay")]
    public class AINodeDecoratorDelay : AINodeDecorator
    {
        public string Delay;

        public override AINodeStatus Update(float ts)
        {
            m_Time += ts;
            if (!m_Blackboard.TryGetValue(Delay, out float delay))
                delay = 0f;

            return m_Time >= delay ? AINodeStatus.Succeeded : AINodeStatus.Running;
        }

        public override void Reset()
        {
            m_Time = 0f;
        }

        private float m_Time = 0f;
    }

    [UIName("Is Blackboard Value Set")]
    public class AINodeDecoratorIsBlackboardValueSet : AINodeDecorator
    {
        public string Key;

        [UIName("Check if set")]
        [Tooltip("If false, it'll do the opposite: check if the key is not set")]
        public bool bCheckIfSet = true;

        [UIName("Abort if changed")]
        [Tooltip("If false, the task won't abort its execution if the value changes")]
        public bool bAbortIfChanged = true;

        public override AINodeStatus Update(float ts)
        {
            if (m_DidRun && !bAbortIfChanged)
            {
                return AINodeStatus.Succeeded;
            }

            bool bCanRun = true;
            bool bHasValue = m_Blackboard.HasValue(Key);
            m_Blackboard.TryGetValue(Key, out object currentVal);

            if (m_DidRun)
            {
                if (!Equals(m_PrevValue, currentVal))
                    return AINodeStatus.Failed;
            }
            else
            {
                m_PrevValue = currentVal;
                bCanRun = bCheckIfSet ? bHasValue : !bHasValue;
            }


            m_DidRun = true;
            return bCanRun ? AINodeStatus.Succeeded : AINodeStatus.Failed;
        }

        public override void Reset()
        {
            m_DidRun = false;
        }

        private bool m_DidRun = false;
        private object m_PrevValue = null;
    }
}
