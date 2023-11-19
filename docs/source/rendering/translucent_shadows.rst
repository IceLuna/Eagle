.. _translucent_shadows:

Translucent Shadows
===================
This feature allows `Translucent` materials to cast shadows. By default, `Translucent` materials receive shadows but the don't cast one.
If you want translucent materials to cast shadows, enable this feature in the `Rendering Settings`.

.. note::

	By default, only `Opaque` and `Masked` materials can cast shadows.

On the image below you can see that translucent objects affect lighting when this feature is enabled.
In this example, the light source is white, and the glass is green. Opacity also affects how much light is passed through.
If it's 1, the light will be completely blocked.
If it's 0, the light won't get dimmer (it'll be full green).


.. figure:: imgs/translucent_shadows.png
    :align: center 

    Translucent shadows

.. note::

	Be aware that translucent materials do not receive shadows from translucent materials even if this feature is enabled.
	And objects that don't cast shadows might receive incorrect translucent shadows.

.. note::

	Enabling `translucent shadows` increases GPU memory usage
