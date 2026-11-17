using System;
using System.Diagnostics;

namespace Eagle
{
    class RuntimeException
    {
        [ThreadStatic]
        private static object s_AlreadyReported;

        [DebuggerHidden]
        internal static bool BreakOnUnhandled(Exception exception)
        {
            if (Debugger.IsAttached)
            {
                s_AlreadyReported = exception;
                Debugger.Break();
            }
            return false;
        }

        public static void OnException(object exception)
        {
            Log.Error("RuntimeException.OnException");
            if (exception != null)
            {
                if (exception is SystemException)
                {
                    var e = exception as SystemException;
                    Log.Error(e.Message);
                    Log.Error(e.Source);
                    Log.Error(e.StackTrace);
                }
            }
            else
            {
                Log.Error("Unknown exception!");
            }

            bool bAlreadyReported = exception != null && ReferenceEquals(exception, s_AlreadyReported);
            s_AlreadyReported = null;

            if (!bAlreadyReported && Debugger.IsAttached)
                Debugger.Break();
        }
    }
}
