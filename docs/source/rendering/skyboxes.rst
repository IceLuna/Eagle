Skyboxes
========
You can use skyboxes so that you have something in the background to render.
Currently, there're two types of skyboxes: `Image-based lighting` (IBL), and `Sky`.

IBL
---
IBL is not just a background, it also lights the scene and it can be seen in the reflections.
Special HDR textures can be used for IBLs, that can be set using :ref:`Scene Settings <scene_settings>` panel in the editor.

.. figure:: imgs/ibl.png
    :align: center

    IBL lights the scene and is reflected

Sky
---
Sky, on the other hand, can be used only for the background (it doesn't light the scene).
There're a few sky settings that you can tweak, such as:

1. Sun position.

2. Sky Intensity.

3. Scattering.

4. Enable/Disable cirrus clouds.

5. Enable/Disable cumulus clouds.

6. Clouds Color.

7. Clouds Intensity.

8. Cirrus Clouds Amount.

9. Cumulus Clouds Amount.

10. Cumulus Clouds Layers.

.. note::

    Use it with caution since it may affect the performance. Especially, if cumulus clouds are used with a bunch of layers.

There's a way to use `Sky` and `IBL` at the same time by setting ``Sky as background`` to true.
In this case, IBL will used only for lighting, and Sky will used for the background.

.. figure:: imgs/sky.png
    :align: center

    Sky with clouds + Directional Light

.. figure:: ../imgs/scene_settings.png
    :align: center

    IBL and Sky Settings

