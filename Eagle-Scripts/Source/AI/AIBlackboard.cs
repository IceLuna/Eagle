using System;
using System.Collections.Generic;

namespace Eagle
{
    // Data storage for tasks to share
    public class AIBlackboard
    {
        public void SetValue<T>(string name, T value)
        {
            m_Values[name] = value;
        }

        // Returns false if there's no value, or it can't be read as `T`. Numbers convert to other number types (for example, an int to a float).
        public bool TryGetValue<T>(string name, out T value)
        {
            if (name != null && m_Values.TryGetValue(name, out var obj))
            {
                if (obj is T t)
                {
                    value = t;
                    return true;
                }

                if (TryConvertNumber(obj, out value))
                    return true;
            }

            value = default;
            return false;
        }

        public T GetValue<T>(string name)
        {
            return GetValue(name, default(T));
        }

        // `fallback` if there's no value, or it can't be read as `T`
        public T GetValue<T>(string name, T fallback)
        {
            return TryGetValue(name, out T value) ? value : fallback;
        }

        public bool HasValue(string name)
        {
            return name != null && m_Values.ContainsKey(name);
        }

        public bool Remove(string name)
        {
            return name != null && m_Values.Remove(name);
        }

        private static bool IsNumber(TypeCode code)
        {
            return code >= TypeCode.SByte && code <= TypeCode.Decimal;
        }

        private static bool TryConvertNumber<T>(object obj, out T value)
        {
            value = default;
            if (obj == null || typeof(T).IsEnum || obj is Enum)
                return false;

            if (!IsNumber(Type.GetTypeCode(typeof(T))) || !IsNumber(Type.GetTypeCode(obj.GetType())))
                return false;

            try
            {
                value = (T)Convert.ChangeType(obj, typeof(T), System.Globalization.CultureInfo.InvariantCulture);
                return true;
            }
            catch (OverflowException)
            {
                return false;
            }
        }

        private readonly Dictionary<string, object> m_Values = new Dictionary<string, object>();
    }
}
