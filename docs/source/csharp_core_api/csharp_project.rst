C# Project
==========

Use this class to get paths to project folders.
C# API functions that require a path, expect it to be relative to the `Content` folder of the project.

Here is the ``Project`` class API.

.. code-block:: csharp
	
    public static class Project
    {
        public static string GetProjectPath(); // The path to the projects root directory
        public static string GetBinariesPath(); // The path to the `Binaries` folder
        public static string GetConfigPath(); // The path to the `Config` folder
        public static string GetContentPath(); // The path to the `Content` folder
        public static string GetCachePath(); // The path to the `Cache` folder
        public static string GetRendererCachePath(); // The path to the `Cache/Renderer` folder
        public static string GetSavedPath(); // The path to the `Saved` folder. Currently, it's not really used
    }

