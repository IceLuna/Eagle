C# Logging
==========

It is a static class that allows you to log messages to the editor.
Log messages of different types are color-coded.

.. code-block:: csharp

    public class Log
    {
        public static extern void Trace(string message);    // White
        public static extern void Info(string message);     // Green
        public static extern void Warn(string message);     // Yellow
        public static extern void Error(string message);    // Red
        public static extern void Critical(string message); // Red
    }
