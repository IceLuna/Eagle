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

Supported nodes:

1. **Select Pose by Bool**. Selects a pose depending on the condition.

.. figure:: ../imgs/editor/assets/animationgraph/selectposebybool.png
   :align: center 

2. **Animation Clip**. Retrieves an animation from an animation asset.

.. figure:: ../imgs/editor/assets/animationgraph/animationclip.png
   :align: center 

3. **Blend Poses**. Linearly blends poses based on the weight. `Weight = 0` is `Pose1`, `Weight = 1` is `Pose2`.

.. figure:: ../imgs/editor/assets/animationgraph/animationclip.png
   :align: center 

4. **Calculate Additive** and **Additive Blend**. These two can be used to generate new animations based on others.
   For example, you have three animations: "Base Idle" pose, "Looking Around" pose, and "Running" pose.
   By using `Calculate Additive` node, you can extract data that's unique to "Looking Around" node. To do so, you need to set "Base Idle" animation as a "Reference" pose, and "Looking Around" as a "Source" Pose.
   Now you've successfully extracted the data required for the mesh to look around. Now by using "Additive Blend" node, you can apply it to other animation.
   In this example, let's apply to "Running" animation. Now, you have a new animation that's running and looking around at the same time.
   `Blend Weight` input of `Additive Blend` node controls how strongly "Additive" pose is applied to "Target" pose.

.. figure:: ../imgs/editor/assets/animationgraph/additive.png
   :align: center 

   Running & looking around.

5. **Filter Bones**. It allows you to filter out bones. For example, if you want to apply an animation to right hand, you can do it by specifying its name here.

.. figure:: ../imgs/editor/assets/animationgraph/filterbones.png
   :align: center 

6. **Math** nodes.

.. figure:: ../imgs/editor/assets/animationgraph/math.png
   :align: center 

7. **Logical** nodes.

.. figure:: ../imgs/editor/assets/animationgraph/logical.png
   :align: center 

8. **Comment** node. Allows you to group nodes and leave a comment.

.. figure:: ../imgs/editor/assets/animationgraph/comment.png
   :align: center 

9. **State Machine**. Allows you to define a transition login for animations.

.. figure:: ../imgs/editor/assets/animationgraph/statemachine.png
   :align: center 

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
