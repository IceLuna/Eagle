﻿using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Transform
    {
        public Vector3 Translation;
        public Vector3 Rotation;
        public Vector3 Scale;
    }
}