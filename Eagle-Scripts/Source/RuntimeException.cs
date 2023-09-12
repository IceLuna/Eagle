using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Eagle
{
    class RuntimeException
    {
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
        }
    }
}
