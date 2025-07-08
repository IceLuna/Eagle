.. _installation_guide:

Installation
============

If you just want to download the engine and use it without doing anything fancy to it, just `download`_ it. Or you can build it `manually`_.

.. _manually:

Building Manually
-----------------
Steps to build the engine:

- Install `VulkanSDK 1.3.283.0 <https://sdk.lunarg.com/sdk/download/1.3.283.0/windows/VulkanSDK-1.3.283.0-Installer.exe>`_. (current version of the engine was tested on it)
- Install ``Visual Studio`` with `C++` and `.NET SDK` support. `.NET` support is required for building scripts core.
- Install ``Git``
- Clone the `repository <https://github.com/iceluna/eagle>`_ using the following command: ``git clone --recursive https://github.com/IceLuna/Eagle``
- In the root directory, open ``scripts`` folder and run ``Win-GenProjects-vs****.bat`` script to generate VS solution.
  Also, run ``Win-SetupFileAssociation.bat`` to associate ``.egproj`` files with the engine so that you can open ``.egproj`` files by double-clicking.
- Go to the root directory, run ``Eagle.sln``, build the solution and run `Eagle-Editor`.

.. figure:: imgs/vs_install.png
   :align: center 

   Visual Studio requirements

.. figure:: imgs/vulkan_install.png
   :align: center 

   Vulkan SDK + Debug symbols

.. note::
	
	It is recommended to restart your PC after installing `Vulkan SDK`. If you didn't restart PC and face any issues, try restarting it and regenerating VS solution.
	
The solution has three configurations: `Debug`; `ReleaseWithDebug`; `Release`.
In `Debug` configuration, all optimizations are disabled, and some Eagle Engine debug tools are enabled. For example, you can use `PhysX Visual Debugger <https://developer.nvidia.com/physx-visual-debugger>`_ to debug physics.
`ReleaseWithDebug` configuration is the same as `Debug` but all optimizations are enabled.
`Release` configuration disables debugging tools and enables all optimizations. Also, the default OS console will be hidden. So, if you want to measure the performance, use `Release` builds.

.. note::
	
	Take a look at ``Eagle/Core/Core.h`` file. It contains some useful defines that might help you to debug.
	For example, there's a ``EG_GPU_MARKERS`` define. When set ot 1, GPU markers will be created so that you can see render-pass names when using debug tools such as `RenderRoc`.
	Enabling ``EG_GPU_MARKERS`` might affect performance.

.. _download:

Download Eagle Engine
---------------------
If you don't want to build the engine manually, you can download pre-built binaries.

Go to `Releases <https://github.com/IceLuna/Eagle/releases>`_ where you will see all engine releases. Each post has files attached to it and
there you'll see a `.zip` archieve with the engine. Download it and unpack it. Go to ``Eagle-Editor`` folder and run the engine (``Eagle-Editor.exe``).

For writing scripts you'll need Visual Studio with `.NET SDK` support.

Go :ref:`here <scripts_guide>` to learn more about writing scripts.
