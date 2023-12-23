.. _scripts_guide:

C# Scripting
============
For programming in-game logic, you can use C#. To use it, you need to install `Visual Studio` and `.NET SDK`.
If everything is set up correctly, you should have ``Sandbox.sln`` file located in ``Sandbox`` folder that you should be able to open in VS. Use that project for all your scripts. If you have some problems, follow installation guide.

.. note::
	
	Your scripts need to be derived from ``Entity`` class. More read about it :ref:`here <csharp_entity_guide>`

.. _coreapi:

C# Core API
-----------
Eagle Engine provides a C# API that can be used for making scripts for a project.

To learn how to use C# with Eagle, check out these pages:

.. toctree::
   :maxdepth: 1

   csharp_core_api/csharp_logging
   csharp_core_api/csharp_project
   csharp_core_api/csharp_scene
   csharp_core_api/csharp_audio
   csharp_core_api/csharp_keycodes
   csharp_core_api/csharp_input
   csharp_core_api/csharp_events
   csharp_core_api/csharp_math
   csharp_core_api/csharp_renderer
   csharp_core_api/csharp_entity
   csharp_core_api/csharp_components

.. _csharpdebugging:

C# Debugging
------------
You can debug C# scripts!
The engine comes with a Visual Studio extension that can be installed to enable debugging of your scripts through Visual Studio.

Step to debug:

- Install ``CSharpDebuggingTool.vsix`` that is located in the root directory
- Run Eagle-Editor
- Open up project's solution file that contains scripts
- In Visual Studio, press ``Debug->Attach Mono Debugger``
