using System.Runtime.CompilerServices;

namespace Eagle
{
    public class Scene
    {
        public static void OpenScene(string path) { OpenScene_Native(path); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void OpenScene_Native(string path);
    }
}
