Screen Space Reflections
========================
Screen Space Reflection (SSR) is a technique for reusing screen space data to calculate reflections.
Since it's a screen space effect, you won't be able to see objects in reflections if they're not on the screen.

.. figure:: imgs/no_ssr.png
    :align: center 

    SSR is disabled

.. figure:: imgs/ssr.png
    :align: center 

    SSR is enabled

.. figure:: imgs/ssr_cutoff.png
    :align: center 

    Reflection is cutoff

You can control the quality of SSR by changing ``Samples per Quad`` and ``Max Traversal Iterations``.
The more, the better, and also slower.

Additionally, you can adjust ``Roughness Threshold`` for SSR. If material's roughness is higher than the threshold, SSR won't be applied.
