using System;

namespace Eagle
{
    // Handles AI tasks execution
    public class AITaskManager
    {
        public void Update(float ts)
        {
            m_Time += ts;

            AITaskManager previous = s_Updating;
            s_Updating = this;
            try
            {
                m_Root?.Run(ts);
            }
            finally
            {
                s_Updating = previous;
            }
        }

        public double GetTime() { return m_Time; }

        // The manager whose tree is being updated
        // Null outside of `Update`
        internal static AITaskManager GetCurrent() {  return s_Updating; }

        public void SetRoot(AINode root)
        {
            m_Root = root;
            m_Root?.SetBlackboard(m_Blackboard);
        }

        public AINode GetRoot() { return m_Root; }

        public void SetBlackboard(AIBlackboard blackboard)
        {
            m_Blackboard = blackboard;
            m_Root?.SetBlackboard(blackboard);
        }

        public AIBlackboard GetBlackboard() { return m_Blackboard; }

        protected AINode m_Root = null;
        protected AIBlackboard m_Blackboard = new AIBlackboard();
        private double m_Time = 0.0;

        [ThreadStatic]
        private static AITaskManager s_Updating;
    }
}
