Other Settings
==============
Here are described some other renderer settings that were not described before.

- `VSync`.

- `Gamma`.

- `Exposure`.

- `Stutterless Mode`. If set to true, information about Point/Spot/Dir lights will be dynamically sent to shaders meaning it won't recompile and won't trigger recompilation.
  But since they'll become dynamic, the compiler won't be able to optimize some shader code making it run slower.
  So if you don't care much about the performance and want to avoid stutters when adding/removing lights, use this option.
  If set to false, adding/removing lights MIGHT trigger some shaders recompilation since some of the light data is getting injected right into the shader source code which then needs to be recompiled.
  But it's not that bad because shaders are being cached. So if the engine sees the same light data again, there'll be no stutters since shaders won't be recompiled, they'll be just taken from the cache.

- `Object Picking`. If your project's runtime doesn't require object picking, disable it to improve performance and reduce memory usage.

- `2D Object Picking`. If set to false, 2D objects will be ignored during object picking. Again, affects only runtime. This setting is ignored if ``Object Picking`` is disabled.

- `Line Width`. Controls the width of rendered lines. The editor uses lines for rendering debug information (for example, bounds of collisions and lights).

- `Grid Scale`. It's purely a parameter for the editor. It allows you to control the scale of the editor grid.
