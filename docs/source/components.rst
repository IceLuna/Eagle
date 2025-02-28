Components
==========
In its core Eagle Engine uses `Entity-Component System` (`ECS <https://en.wikipedia.org/wiki/Entity_component_system>`_) approach.

An entity represents a general-purpose object. For example, every game object is represented as an entity.

A component labels an entity as possessing a particular aspect, and holds the data needed to model that aspect. For example, every game object that can take damage might have a `Health` component associated with its entity.

.. note::

   An entity can only have one component of a given type attached. For example, an entity can't have two ``Static Mesh`` components.

Camera Component
----------------
Represents a camera that can be used for in-game rendering.

Camera Component has the following parameters:

1. `Is Primary`. A scene can have multiple cameras, but which one to use is determined by this flag. If it's set to true, the camera will be enabled.

2. `Projection`. It's either `perspective` or `orthographic`.

3. `FOV`. Field of view.

4. `Near Clip`. Everything closer will be clipped.

5. `Far Clip`. Everything further will be clipped.

6. `Shadow Far Clip`. Beyond this distance shadows will disappear.

7. `Cascades Split Alpha`. It's used to determine how to split shadow cascades for directional light shadows.

8. `Cascades Smooth Transition Alpha`. The blend amount between cascades of directional light shadows. Try to keep it as low as possible.

.. note::

	It doesn't have any input handling. All of that needs to be done manually using C# scripts.

.. figure:: imgs/components/camera.png
   :align: center 

   Camera Component

C# Script Component
-------------------
This components allows you to attach a C# script to an entity.

In the editor, there's a ``Script Class`` drop down menu that contains a list of all your scripts where you can select one.
When a script is selected, the editor displays public values from it allowing you to change their default values without modifying scripts.
Currently, the editor only exposes the following types: ``bool``, ``int``, ``uint``, ``float``, ``string``, ``Vector2``, ``Vector3``, ``Vector4``, ``Color3``, ``Color4``, ``Enum`` (any enum), ``Entity``, ``Asset``.
When you use a ``public Entity entity``, in the editor you'll be able to select an entity from the scene.

.. figure:: imgs/components/script.png
   :align: center 

   C# Script Component

Sprite Component
----------------
It allows you to render 2D textures that are lit (goes through the PBR pipeline).

Sprite Component has the following parameters:

1. `Casts Shadows`.

2. `Is Atlas`. If it's set to true, you can use an atlas for rendering, and the following additional parameters appear:

   a) `Sprite Coords`. It's a sprite index within an atlas (starting from top left corner in 0;0). For example, if an atlas is 128x128 and a sprite has a 32x32 size, and in case you want to select a sprite at 64x32, here you enter 2x1.

   b) `Sprite Size`. Defines a size of a single sprite in the atlas.

   c) `Sprite Size Coef`. In case some sprites have different sizes so this paramater allows you to adjust for that case.

3. `Material`.

4. `Receives Decals`. If set to false, decals won't be projected on this surface.

.. figure:: imgs/components/sprite1.png
   :align: center 

   Sprite Component

.. figure:: imgs/components/sprite2.png
   :align: center 

   Sprite Component. Atlas

Static Mesh Component
---------------------
It allows you to render meshes.

Static Mesh Component has the following parameters:

1. `Static Mesh`.

2. `Casts Shadows`.

3. `Materials`.

4. `Receives Decals`. If set to false, decals won't be projected on this surface.

.. figure:: imgs/components/static_mesh.png
   :align: center 

   Static Mesh Component

Skeletal Mesh Component
-----------------------
It allows you to render animated meshes.

Skeletal Mesh Component has the following parameters:

1. `Skeletal Mesh`.

2. `Casts Shadows`.

3. `Receives Decals`. If set to false, decals won't be projected on this surface.

4. `Materials`.

5. `Ragdolling`. If set to true, skeletal mesh will be switched to ragdoll state.

6. `Animation Type`. Can be either `Clip` or `Graph`.

7. `Root Motion Lock Position`. Allows to limit root motion's effect.

8. `Animation`. Either an `Animation` or `Animation Graph`.

9. `Graph Variables`. If `Animation Graph` is used, its variables will be displayed that you can modify.
   Each component has its own copy of variables, so changing them won't affect other components.

.. figure:: imgs/components/skeletal_mesh.png
   :align: center 

   Skeletal Mesh Component

Particle System Component
-------------------------
It allows you to render particles.

Particle System Component has the following parameters:

1. `Particle System`.

2. `Auto-spawn`. If set to true, particle system will be spawned automatically. Otherwise, you need to do it manually using C# scripts.

.. figure:: imgs/components/particle_system.png
   :align: center 

   Particle System Component

Billboard Component
-------------------
It represents a texture that always faces the camera. It doesn't account for lighting.

Billboard Component just has a `Texture` parameter.

.. figure:: imgs/components/billboard.png
   :align: center 

   Billboard Component

Decal Component
---------------
It represents a material that can be projected on other materials.

Decal Component has the following parameters:

1. `Material`. Currently, decals ignore normal input of materials.

2. `Sort Priority`. Higher value results in Decal being draw on top of others.

3. `Adjust Aspect Ratio`. Aspect Ratio will be adjusted according to `Albedo` texture.

.. figure:: imgs/components/decal.png
   :align: center 

   Decal Component

Text Component
--------------
Allows you to render 3D Text.

Text Component has the following parameters:

1. `Font`.

2. `Text`. Use ``Ctrl+Enter`` to drop to a new line.

3. `Casts Shadows`.

4. `Is Lit`. If it's set to true, the component will react to lighting and will be lit correspondingly. When checked, additional material input parameters will appear.

5. `Color`. An HDR value that represent a color of the text. If ``Is Lit`` set to true, it's ignored and material inputs are used instead.

6. `Line Spacing`. Spacing between lines.

7. `Kerning`. Spacing between letters.

8. `Max Width`. The maximum width of a line.

9. `Receives Decals`. If set to false, decals won't be projected on this surface.

.. figure:: imgs/components/text.png
   :align: center 

   Text Component

Text2D Component
----------------
Allows you to render screen-space 2D Text (useful for in-game UI).

Text2D Component has the following parameters:

1. `Font`.

2. `Text`. Use ``Ctrl+Enter`` to drop to a new line.

3. `Color`. An HDR value that represent a color of the text.

4. `Position`. It's normalized device coords and it is the position of the top left vertex of the first symbol. Text2D will try to be at the same position of the screen no matter the resolution and occupy the same amount of space.
   `(-1; -1)` is the top left corner of the screen; `(0; 0)` is the center; `(1; 1)` is the bottom right corner of the screen.

5. `Scale`.

6. `Rotation`.

7. `Opacity`. Controls the translucency of the text.

8. `Is Visible`.

9. `Line Spacing`. Spacing between lines.

10. `Kerning`. Spacing between letters.

11. `Max Width`. Maximum width of a line.

.. figure:: imgs/components/text2D.png
   :align: center 

   Text2D Component

Image2D Component
-----------------
Allows you to render textures in screen-space (useful for in-game UI).

Image2D Component has the following parameters:

1. `Texture`.

2. `Tint`. An HDR value that allows you to tint your texture.

3. `Position`. It's normalized device coords and it is the position of the top left vertex of the first symbol. Image2D will try to be at the same position of the screen no matter the resolution and occupy the same amount of space.
   `(-1; -1)` is the top left corner of the screen; `(0; 0)` is the center; `(1; 1)` is the bottom right corner of the screen.

4. `Scale`.

5. `Rotation`.

6. `Opacity`. Controls the translucency of the image.

7. `Is Visible`.

.. note::

	`Text2D` is rendered on top of `Image2D`.

.. figure:: imgs/components/image2D.png
   :align: center 

   Image2D Component


Audio Component
---------------
It allows you to play 3D sounds.

Audio Component has the following parameters:

1. `Audio`.

2. `Is 3D`.

3. `Roll off`. It allows you to specify how 3D sounds attenuate as the distance between the listener and the sound increases. You can set one of the following values:

   a) `Linear`. It means that a sounds will follow a linear roll off model where ``MinDistance`` = full volume, ``MaxDistance`` = silence

   b) `Inverse`. It means that a sounds will follow an inverse roll off model where ``MinDistance`` = full volume, ``MaxDistance`` = where sound stops attenuating, and roll-off is fixed according to the global roll off factor.

   c) `LinearSquare`. It means that a sounds will follow a linear-square roll off model where ``MinDistance`` = full volume, ``MaxDistance`` = silence

   d) `InverseTapered`. It means that a sounds will follow the inverse roll off model at distances close to ``MinDistance`` and a linear-square roll off close to ``MaxDistance``

4. `Volume`.

5. `Pitch`. A value between `0.0` and `10.0`.

6. `Pan`. A value between `-1.0` and `1.0`. `-1` = Completely on the left. `+1` = Completely on the right.

7. `Loop Count`. ``-1`` = ``Loop Endlessly``; ``0`` = ``Play once``; ``1`` = ``Play twice``, etc...

8. `Min Distance`. The minimum distance is the point at which the sound starts attenuating. If the listener is any closer to the source than the minimum distance, the sound will play at full volume.

9. `Max Distance`. The maximum distance is the point at which the sound stops attenuating and its volume remains constant (a volume which is not necessarily zero).

10. `Is Looping`.

11. `Is Streaming`. When you stream a sound, you can only have one instance of it playing at any time. This limitation exists because there is only one decode buffer per stream. As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks, while most sound effects should be loaded into memory.

12. `Is Muted`.

13. `FFT Samples`. Number of samples in the output of spectrum data. Must be the power of 2 between 64 and 8192.

14. `FFT Type`. Defines the method that will be used to calculate the spectrum data. https://www.fmod.com/docs/2.00/api/core-api-common-dsp-effects.html#fmod_dsp_fft

15. `FFT Enabled`. If set to true, you can get the spectrum data of the sound.

16. `Autoplay`. The sound will autoplay when spawned during the simulation.

17. `Enable Doppler Effect`. You can learn more about it `here <https://en.wikipedia.org/wiki/Doppler_effect>`_.

.. figure:: imgs/components/audio.png
   :align: center 

   Audio Component

Reverb Component
----------------
It allows you to play apply effect to 3D sounds.

Reverb Component has the following parameters:

1. `Preset`. It allows you to simulate different types of environments. You can set one of the following values: `Generic`, `Padded Cell`, `Room`, `Bathroom`, `Living Room`, `Stone Room`, `Auditorium`, `Concert Hall`,
   `Cave`, `Arena`, `Hangar`, `Carpetted Hallway`, `Hallway`, `Stone Corridor`, `Alley`, `Forest`, `City`, `Mountains`, `Quarry`, `Plain`, `Parking Lot`, `Sewer Pipe`, `Under Water`.

2. `Min Distance`. Reverb is at full volume within that radius.

3. `Max Distance`. Reverb is disabled outside that radius.

4. `Is Active`.

5. `Visualize Radius`.

.. figure:: imgs/components/reverb.png
   :align: center 

   Reverb Component

.. _nav_mesh_component:

Navigation Mesh Component
-------------------------
It allows you to build navigation mesh that can be used by your AI scripts to move from one point to another while avoiding obstacles.

.. note::

	Currently, a scene can only have one active navigation mesh.

Navigation Mesh Component has the following parameters:

1. `Build`. Allows you to build it. Can be used to switch between nav meshes since a scene supports only one active nav mesh.

2. `Auto Rebuild`. If set to true, nav mesh is rebuilt automatically when its transform or settings are changed.

3. `Crowd Settings`. Controls limits of crowd agents. See :ref:`Navigation Crowd Agent Component <nav_crowd_agent_component>`

   a) `Max Agents`.

   b) `Max Agent Radius`.

4. `Nav Mesh Settings`.

   a) `AABB`. Controls nav mesh extents

   b) `Max Query Nodes`. Maximum number of search nodes. [Limits: ``0 < value <= 65535``].

   c) `Expected Layers per Tile`.

   d) `Max Layers`.

   e) `Max Obstacles`.

   f) `Tile Size`. The width/height size of tile's on the xz-plane.

   g) `Cell Size`. The xz-plane cell size to use for fields.

   h) `Cell Height`. The y-axis cell size to use for fields.

   i) `Max Slope`. The maximum slope that is considered walkable.

   j) `Agent Height`. Minimum floor to `ceiling` height that will still allow the floor area to be considered walkable.

   k) `Agent Max Climb`. Maximum ledge height that is considered to still be traversable.

   l) `Agent Radius`. The distance to erode/shrink the walkable area of the heightfield away from obstructions.

   m) `Edge Max Len`. The maximum allowed length for contour edges along the border of the mesh.

   n) `Edge Max Error`. The maximum distance a simplified contour's border edges should deviate the original raw contour.

   o) `Region Min Size`. The minimum number of cells allowed to form isolated island areas

   p) `Region Merge Size`. Any regions with a span count smaller than this value will, if possible, be merged with larger regions.

   q) `Verts Per Poly`. The maximum number of vertices allowed for polygons generated during the contour to polygon conversion process.

   r) `Border Size`. The size of the non-navigable border around the heightfield.

   s) `Filter Low Hanging Obstacles`. Marks non-walkable spans as walkable if their maximum is within AgentMaxClimb of the span below them.
      This removes small obstacles and rasterization artifacts that the agent would be able to walk over such as curbs. It also allows agents to move up terraced structures like stairs.

   t) `Filter Ledge Spans`. Marks spans that are ledges as not-walkable. A ledge is a span with one or more neighbors whose maximum is further away than AgentMaxClimb from the current span's maximum.
      This method removes the impact of the overestimation of conservative voxelization so the resulting mesh will not have regions hanging in the air over ledges.

   u) `Filter Walkable Low Height Spans`. Marks walkable spans as not walkable if the clearance above the span is less than the specified `Agent Height`. For this filter, the clearance above the span is the distance
      from the span's maximum to the minimum of the next higher span in the same column. If there is no higher span in the column, the clearance is computed as the distance from the top of the span to the maximum heightfield height.

.. figure:: imgs/components/nav_mesh.png
   :align: center 

   Navigation Mesh Component

.. figure:: imgs/components/nav_mesh_example.gif
    :align: center 

    Navigation Mesh in action

.. _nav_crowd_agent_component:

Navigation Crowd Agent Component
--------------------------------
It allows you mark entities as crowd agents. And the benefit of it is that you can easily control a lot of NPCs and change their move target.

For example, you can create 100 crowd agents, and make all of them to move to a single point by simply calling one C# function: ``CrowdNavigation.SetMoveTarget()``.
Of cource, you can also control each agent individually.

Navigation Crowd Agent Component has the following parameters:

1. `Agent Radius`.

2. `Agent Height`.

3. `Max Acceleration`.

4. `Max Speed`.

5. `Separation Weight`.

6. `Obstacle Avoidance Quality`.

7. `Anticipate Turns`.

8. `Optimize Path Visibility`.

9. `Optimize Path Topology`.

10. `Crowd Separation`.

.. figure:: imgs/components/nav_crowd_agent.png
   :align: center 

   Navigation Crowd Agent Component

.. note::

    You don't have to use crowd agents to move your entities.
    If you want to do it manually, you can call C# functions such as ``Navigation.FindStraightPath()`` to get an array of points you need to move to.

Rigid Body Component
--------------------
It represents a physics object, and each physics object must have it.
It can either be `static` or `dynamic`. `Static` type represents an immovable object meaning it won't react to any forces applied to it.

Rigid Body Component has the following parameters:

1. `Body Type`. It's either `static` or `dynamic`.

2. `Collision Detection`. It's either `Discrete`, `Continuous`, or `Continuous Speculative`. When continuous collision detection (or CCD) is turned on, the affected rigid bodies will not go through other objects at high velocities (a problem also known as `tunnelling <https://gamedev.stackexchange.com/questions/192400/in-games-physics-engines-what-is-tunneling-also-known-as-the-bullet-through>`_).
   A cheaper but less robust approach is called speculative CCD.

3. `Mass`. If the mass is set to 0, the body will have infinite mass so its linear velocity cannot be changed by any constraints.

4. `Linear Damping`.

5. `Angular Damping`.

6. `Max Linear Velocity`.

7. `Max Angular Velocity`.

8. `Is Gravity Enabled`.

9. `Is Kinematic`. Sometimes controlling an actor using forces or constraints is not sufficiently robust, precise or flexible. For example, moving platforms or character controllers often need to manipulate an actor's position or make it exactly follow a specific path.
   Such a control scheme is provided by kinematic actors. A kinematic actor is controlled using the ``SetKinematicTarget`` function. Each simulation step PhysX moves the actor to its target position, regardless of external forces, gravity, collision, etc.
   Thus, one must continually call ``SetKinematicTarget``, every time step, for each kinematic actor, to make them move along their desired paths. The movement of a kinematic actor affects dynamic actors with which it collides or to which it is constrained with a joint.
   The actor will appear to have infinite mass and will push regular dynamic actors out of the way.

10. `Lock Position`. Can be used to prevent physics system from changing position.

11. `Lock Rotation`. Can be used to prevent physics system from changing rotation.

.. figure:: imgs/components/rigidbody.png
   :align: center 

   Rigid Body Component

Box Collider Component
----------------------
It represents a physics collider that has the shape of a box.

Box Collider Component has the following parameters:

1. `Physics Material`.

2. `Size`. XYZ-size of the box colider.

3. `Is Trigger`. Its role is to report that there has been an overlap with another shape. Trigger shapes play no part in the simulation of the scene.

4. `Is Obstacle`. Can be used for AI Navigation to block the path. Note: only box obstacles react to rotation (along Y), other obstacles don't rotate!

5. `Affects NavMesh`. If set to false, it won't affect NavMesh builds. It still can be used as an obstacle though.

6. `Is Collision Visible`. Can be used to visualize collision bounds.

.. figure:: imgs/components/box_collider.png
   :align: center 

   Box Collider Component

Sphere Collider Component
-------------------------
It represents a physics collider that has the shape of a sphere.

Sphere Collider Component has the following parameters:

1. `Physics Material`.

2. `Radius`. Radius of the sphere collider.

3. `Is Trigger`. Its role is to report that there has been an overlap with another shape. Trigger shapes play no part in the simulation of the scene.

4. `Is Obstacle`. Can be used for AI Navigation to block the path. Note: only box obstacles react to rotation (along Y), other obstacles don't rotate!

5. `Affects NavMesh`. If set to false, it won't affect NavMesh builds. It still can be used as an obstacle though.

6. `Is Collision Visible`. Can be used to visualize collision bounds.

.. figure:: imgs/components/sphere_collider.png
   :align: center 

   Sphere Collider Component

Capsule Collider Component
--------------------------
It represents a physics collider that has the shape of a capsule.

Capsule Collider Component has the following parameters:

1. `Physics Material`.

2. `Radius`. Radius of the capsule collider.

3. `Height`. Height of the capsule collider.

4. `Is Trigger`. Its role is to report that there has been an overlap with another shape. Trigger shapes play no part in the simulation of the scene.

5. `Is Obstacle`. Can be used for AI Navigation to block the path. Note: only box obstacles react to rotation (along Y), other obstacles don't rotate!

6. `Affects NavMesh`. If set to false, it won't affect NavMesh builds. It still can be used as an obstacle though.

7. `Is Collision Visible`. Can be used to visualize collision bounds.

.. figure:: imgs/components/capsule_collider.png
   :align: center 

   Capsule Collider Component

Mesh Collider Component
-----------------------
It represents a physics collider that has the shape of a mesh.

Mesh Collider Component has the following parameters:

1. `Collision Mesh`. A mesh to create a collider from.

2. `Physics Material`.

3. `Is Trigger`. Its role is to report that there has been an overlap with another shape. Trigger shapes play no part in the simulation of the scene.

4. `Is Collision Visible`. Can be used to visualize collision bounds.

5. `Is Convex`. When set to true, collider will be created using a rough approximation of the mesh. Non-convex mesh collider can be used only with kinematic or static actors.

6. `Is two-sided`. Only affects non-convex mesh colliders. Non-convex meshes are one-sided meaning collision won't be registered from the back side. For example, that might be a problem for windows.
   So to fix this problem, you can set this flag to true

7. `Affects NavMesh`. If set to false, it won't affect NavMesh builds. It still can be used as an obstacle though.

.. figure:: imgs/components/mesh_collider.png
   :align: center 

   Mesh Collider Component

Point Light Component
---------------------
It allows you to use `point` light sources.

Point Light Component has the following parameters:

1. `Light Color`.

2. `Intensity`.

3. `Attenuation Radius`. Can be used to limit light's influence. Make it as small as possible for better performance.

4. `Affects world`. Can be used to completely disable a light.

5. `Casts shadows`. Whenever you don't really need it, disable it to save on performance and GPU memory.

6. `Visualize Radius`.

7. `Is Volumetric`. When set to true, it's performance intensive. For it to account for object interaction, light needs to cast shadows. If you want to use it, enable `Volumetric Light` feature in Renderer Settings.

8. `Volumetric Fog Intensity`.

.. figure:: imgs/components/point_light.png
   :align: center 

   Point Light Component

Spot Light Component
--------------------
It allows you to use `spot` light sources.

Spot Light Component has the following parameters:

1. `Light Color`.

2. `Intensity`.

3. `Attenuation Distance`.  Can be used to limit light's influence. Make it as small as possible for better performance.

4. `Inner Angle`.

5. `Outer Angle`.

6. `Affects world`. Can be used to completely disable a light.

7. `Casts shadows`. Whenever you don't really need it, disable it to save on performance and GPU memory.

8. `Visualize Distance`.

9. `Is Volumetric`. When set to true, it's performance intensive. For it to account for object interaction, light needs to cast shadows. If you want to use it, enable `Volumetric Light` feature in Renderer Settings.

10. `Volumetric Fog Intensity`.

.. figure:: imgs/components/spot_light.png
   :align: center 

   Spot Light Component

Directional Light Component
---------------------------
It allows you to use `directional` light sources.

Directional Light Component has the following parameters:

1. `Light Color`.

2. `Intensity`.

3. `Ambient`. It can be used to light parts of the scene that aren't directly seen by it.

4. `Affects world`. Can be used to completely disable a light.

5. `Casts shadows`. Whenever you don't really need it, disable it to save on performance and GPU memory.

6. `Visualize Direction`.

7. `Is Volumetric`. When set to true, it's performance intensive. For it to account for object interaction, light needs to cast shadows. If you want to use it, enable `Volumetric Light` feature in Renderer Settings.

8. `Volumetric Fog Intensity`.

.. figure:: imgs/components/dir_light.png
   :align: center 

   Directional Light Component
