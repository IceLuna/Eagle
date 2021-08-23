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
            Console.WriteLine("RuntimeException.OnException");
            if (exception != null)
            {
                if (exception is NullReferenceException)
                {
                    var e = exception as NullReferenceException;
                    Console.WriteLine(e.Message);
                    Console.WriteLine(e.Source);
                    Console.WriteLine(e.StackTrace);
                }
            }
            else
            {
                Console.WriteLine("Unknown exception!");
            }
        }
    }
}
