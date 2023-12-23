using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    public class Log
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Trace(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Info(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Warn(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Error(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Critical(string message);
    }
}
