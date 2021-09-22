using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct GUID
    {
        private ulong m_Higher64;
        private ulong m_Lower64;
    }
}
