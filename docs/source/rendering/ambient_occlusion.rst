Ambient Occlusion
=================
Eagle Engine supports `SSAO` (screen-space ambient occlusion) and `GTAO` (ground-truth ambient occlusion).
These techniques are used for efficiently approximating the ambient occlusion effect in real time.

.. note::

	They do nothing if there's no ambient lighting (for example, IBL). It's also affected by material's ``Ambient Occlusion`` input.

SSAO
----

For SSAO, you can change the following settings:

1. `Samples`. Affects how many samples (points) to consider when calculating an ambient occlusion. The higher, the better. But be careful, since making it too high will affect performance.

2. `Radius`. Occluders search radius

3. `Bias`. It is a scalar that affects the calculation that determines if a sample is an occluder.

GTAO
----
GTAO has two settings: `Samples`, and `Radius`. They have the same meaning as for SSAO.

.. note::

	Currently, GTAO is not production ready. It doesn't always work as intented.

.. figure:: imgs/ao.png
    :align: center 

    Different AO techniques
