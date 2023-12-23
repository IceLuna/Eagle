Translucency
============
Translucency is all about objects (or parts of them) not having a solid color, but having a combination of colors from the object itself and any other object behind it with varying intensity.
A colored glass window is a transparent object; the glass has a color of its own, but the resulting color contains the colors of the objects behind the glass as well

Support for translucency is implemented using a technique called `Order-independent transparency` (OIT).
It is a technique which doesn't require us to draw our transparent objects in an orderly fashion.

You can set whether material is translucent or not by changing ``Blend mode`` parameter of a material to be ``Translucent``. Material's ``Opacity`` input can be used to control translucency.

The engine exposes a setting called ``Transparency Layers`` which lets you control how good translucent object mix their colors when they're stacked behind each other.
Use this setting with caution because it affects memory usage. Memory consumption in `bytes` can be calculated by this formula: ``Memory consumption = viewport_width * viewport_height * layers * 12``.

.. note::

    Translucent materials receive shadows but, by default, they don't cast one. If you want them to cast shadows, enable :ref:`translucent shadows <translucent_shadows>` feature.

.. figure:: imgs/opacity.png
    :align: center 

    Translucent green window
