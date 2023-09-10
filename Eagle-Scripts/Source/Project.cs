using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace Eagle
{
    public static class Project
    {
        public static string GetProjectPath() { return GetProjectPath_Native(); }

        public static string GetContentPath() { return GetContentPath_Native(); }

        public static string GetCachePath() { return GetCachePath_Native(); }

        public static string GetRendererCachePath() { return GetRendererCachePath_Native(); }

        public static string GetSavedPath() { return GetSavedPath_Native(); }


        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetProjectPath_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetContentPath_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetCachePath_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetRendererCachePath_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetSavedPath_Native();
    }
}
