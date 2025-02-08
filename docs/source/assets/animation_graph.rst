.. _asset_animation_graph:

Animation Graph asset
=====================

This asset type represents an animation graph which you can modify by using `Animation Graph Editor`.
It allows you to create complex animation logic by usign nodes.

.. figure:: ../imgs/editor/assets/animationgraph/animationgraph.png
   :align: center 

   Animation Graph Editor

|

It allows you to create variables of four types: `Bool`, `Float`, `String`, and `Animation`. All variables can be changed during runtime and accessed in C#.
You can use this asset in `SkeletalMesh` component. Note that each component has its own copy of the graph. So, changing variables in one component won't affect the others.
To break a node link/connection, left-click it while holding `Alt`.

Here's the list of supported nodes:

Select Pose by Bool
-------------------
It selects a pose depending on the condition.

.. figure:: ../imgs/editor/assets/animationgraph/selectposebybool.png
   :align: center 
   
|

Animation Clip
--------------
It retrieves an animation from an animation asset.

.. figure:: ../imgs/editor/assets/animationgraph/animationclip.png
   :align: center 
   
|

State Machine
-------------
It allows you to define a transition logic for animations.

.. figure:: ../imgs/editor/assets/animationgraph/statemachine.png
   :align: center 
   
|

By clicking on a state itself, you can define an animation that will be played when the state is active.
You can also specify transition settings, such as: transition condition, transition time, smooth transition.

.. figure:: ../imgs/editor/assets/animationgraph/simple_statemachine.png
   :align: center 

   State Machine example

.. figure:: ../imgs/editor/assets/animationgraph/idle_state.png
   :align: center 

   Idle state

.. figure:: ../imgs/editor/assets/animationgraph/walk_state.png
   :align: center 

   Walk state

.. figure:: ../imgs/editor/assets/animationgraph/run_state.png
   :align: center 

   Run state

.. figure:: ../imgs/editor/assets/animationgraph/idle_to_walk.png
   :align: center 

   Idle to Walk transition

.. figure:: ../imgs/editor/assets/animationgraph/walk_to_idle.png
   :align: center 

   Walk to Idle transition

.. figure:: ../imgs/editor/assets/animationgraph/walk_to_run.png
   :align: center 

   Walk to Run transition

.. figure:: ../imgs/editor/assets/animationgraph/run_to_walk.png
   :align: center 

   Run to Walk transition

Blend Poses
-----------
Linearly blends poses based on the weight. `Weight = 0` is `Pose1`, `Weight = 1` is `Pose2`.

.. figure:: ../imgs/editor/assets/animationgraph/animationclip.png
   :align: center 
   
|

Additive blending
-----------------
**Calculate Additive** and **Additive Blend**. These two can be used to generate new animations based on others.
For example, you have three animations: "Base Idle" pose, "Looking Around" pose, and "Running" pose.
By using `Calculate Additive` node, you can extract data that's unique to "Looking Around" pose. To do so, you need to set "Base Idle" animation as a "Reference" pose, and "Looking Around" as a "Source" Pose.
Now you've successfully extracted the data required for the mesh to look around. Now, by using "Additive Blend" node, you can apply it to other animations.
In this example, let's apply it to "Running" animation. Now, you have a new animation that's running and looking around at the same time.
`Blend Weight` input of `Additive Blend` node controls how strongly "Additive" pose is applied to "Target" pose.

.. figure:: ../imgs/editor/assets/animationgraph/additive.png
   :align: center 

   Running & looking around.

Filter Bones
------------
It allows you to filter out bones. For example, if you want to apply an animation to right hand only, you can do it by specifying its name here.

.. figure:: ../imgs/editor/assets/animationgraph/filterbones.png
   :align: center 
   
|

Math nodes
----------

.. figure:: ../imgs/editor/assets/animationgraph/math.png
   :align: center 
   
|

Logical nodes
-------------

.. figure:: ../imgs/editor/assets/animationgraph/logical.png
   :align: center 
   
|

Comment node
------------
It allows you to group nodes and leave a comment.

.. figure:: ../imgs/editor/assets/animationgraph/comment.png
   :align: center 
   
|
