Demo
====
By default, the engine comes with a project where you can run around, jump, shoot, change renderer settings, and even teleport to a different scene.

You can use it as a starting point to learn about using the engine.

.. note::
    IBL is disabled by default to reduce gpu memory usage. That's why in-game `IBL` switch won't work.
    Enabling IBL will fix it. (`Eagle/Sandbox/Content/Textures/IBL/limpopo_golf_course_4k.hdr`)

.. note::
   If you can't run demo project because of lack of GPU memory, open ``Eagle/Sandbox/Engine/EditorDefault.ini`` file,
   and set ``TranslucentShadows`` to false, and also set ``Volumetric Light Settings -> bEnable`` to false.
   You can additionally try to reduce resolution of shadow maps.

.. figure:: imgs/demo_scene.png
    :align: center 

    Demo project

.. figure:: imgs/demo_runtime.png
    :align: center 

    Demo runtime
