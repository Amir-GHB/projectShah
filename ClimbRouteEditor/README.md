# Climbing Route Authoring v2

This is the first authoring foundation for the new climbing route system. It is developed on the
`feature/climbing-route-authoring-v2` branch and intentionally leaves the legacy `AClimbRoute`
and `UClimbingComponent` implementation in place while the replacement is built.

## Data model

- `AClimbRouteActor` owns one network.
- `FClimbRouteNode` represents meaningful contact locations and uses stable GUIDs.
- `FClimbRouteSection` connects two nodes through an `FClimbPathDefinition`.
- `FClimbPathDefinition` currently represents straight polyline segments, with optional intermediate points.
- `FClimbRouteTraversalLink` represents jumps, drops, mantles, and other special actions.

The authoring path does not use `USplineComponent`. The reserved curved interpolation enum is a future extension point only.

## Editor mode prototype

1. Compile the project with Unreal Engine 5.8.
2. Open the Unreal Editor and place an `AClimbRouteActor` in the level.
3. Select that actor.
4. Activate the **Climb Route** mode from the editor Modes menu.
5. Click near an existing node to select it as the source. The selected source is drawn larger and yellow.
6. Use the keyboard:
   - `N`: toggle isolated-node placement.
   - `P`: toggle straight-section endpoint placement.
   - `J`: toggle jump destination placement.
   - `D`: toggle drop destination placement.
   - `M`: toggle mantle destination placement.
   - `Escape`: cancel the current placement mode.
7. While a placement mode is active, click valid collision geometry. The contact frame is aligned to the hit surface normal, with a stable up-axis fallback and a 5 cm default outward offset.
8. The tool continues from the newly created node, making it possible to build a chain of sections or traversal links.

The current prototype uses the visibility collision channel and complex traces. Geometry must have collision that blocks that channel.

## Current scope and limitations

- Straight paths only; curved interpolation is not implemented.
- The first editor mode provides node/edge visualization and click-to-place interaction.
- Contextual directional buttons, drag handles, ghost character preview, skeletal preview, and runtime graph compilation are subsequent milestones.
- The route actor validates graph references but does not yet perform gameplay reachability analysis.
- This branch has not been compiled against an installed Unreal Engine in this environment. Compile and smoke-test in UE 5.8 before merging.
