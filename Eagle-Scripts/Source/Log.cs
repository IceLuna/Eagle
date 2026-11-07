using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public class Log
    {
        public static void Trace(string message)
        {
            Trace_Native(message);
            SendToDebugger("Trace", message);
        }

        public static void Info(string message)
        {
            Info_Native(message);
            SendToDebugger("Info", message);
        }

        public static void Warn(string message)
        {
            Warn_Native(message);
            SendToDebugger("Warn", message);
        }

        public static void Error(string message)
        {
            Error_Native(message);
            SendToDebugger("Error", message);
        }

        public static void Critical(string message)
        {
            Critical_Native(message);
            SendToDebugger("Critical", message);
        }

        // Shows the message in the Output window of an attached debugger
        private static void SendToDebugger(string level, string message)
        {
            if (Debugger.IsAttached)
                Debugger.Log(0, level, "[" + level + "] " + message + "\n");
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Trace_Native(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Info_Native(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Warn_Native(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Error_Native(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Critical_Native(string message);
    }
}
