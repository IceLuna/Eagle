using System;
using System.Collections.Generic;

namespace Eagle.AI
{
    // Data storage for tasks to share
    public class AIBlackboard
    {
        public void SetValue<T>(string name, T value)
        {
            m_Values[name] = value;
        }

        public bool TryGetValue<T>(string name, out T value)
        {
            if (m_Values.TryGetValue(name, out var obj) && obj is T t)
            {
                value = t;
                return true;
            }

            value = default;
            return false;
        }

        public T GetValue<T>(string name)
        {
            if (m_Values.TryGetValue(name, out var obj) && obj is T t)
            {
                return t;
            }

            return default;
        }

        public bool HasValue(string name)
        {
            return m_Values.ContainsKey(name);
        }

        public bool Remove(string name)
        {
            return m_Values.Remove(name);
        }

        private readonly Dictionary<string, object> m_Values = new Dictionary<string, object>();
    }
}
