Text Rendering
==============
Eagle Engine supports font rendering using a technique called `Multi-channel Signed Distance Field` (MSDF).
Using MSDF allows to render text efficiently at almost any size including extremely large text without any pixelization.

Text has the following parameters you can change:

1. `Does Cast Shadows`.

2. `Is Lit`. If set to true, text will be lit using PBR-pipeline, meaning material will be used for rendering.

3. `Color`. It's used, if ``Is Lit`` is set to false.

4. `Line spacing`. Spacing between lines.

5. `Kerning`. Spacing between letters.

6. `Max Width`. The maximum width of a line.

.. figure:: imgs/text.png
    :align: center 

    Text rendering
