using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct GUID
    {
        private ulong m_Higher64;
        private ulong m_Lower64;

        public bool IsNull() { return m_Higher64 == 0 && m_Lower64 == 0; }

        public static GUID Null()
        {
            GUID result = new GUID();
            result.m_Higher64 = 0;
            result.m_Lower64 = 0;
            return result;
        }

        public override string ToString()
        {
            return $"GUID[{m_Higher64}, {m_Lower64}]";
        }

        public override bool Equals(object obj) => obj is GUID other && this.Equals(other);

        public bool Equals(GUID right) => m_Higher64 == right.m_Higher64 && m_Lower64 == right.m_Lower64;

        public static bool operator ==(GUID left, GUID right) => left.Equals(right);
        public static bool operator !=(GUID left, GUID right) => !(left == right);

        public override int GetHashCode() => (m_Higher64, m_Lower64).GetHashCode();
    }
}
