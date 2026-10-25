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
5. Click near an existing node to choose it as the source. The selected source is drawn larger and yellow.
6. Color-coded action handles appear around each node:
   - Blue handles on either side: create a straight section.
   - Orange handle pointing outward: create a jump link.
   - Green upper handle: create a mantle link.
   - Purple lower handle: create a drop link.
7. Clicking a handle selects its source node and enters destination-placement mode. Click valid geometry to create the destination.
8. Keyboard alternatives: `N` toggles isolated-node placement, `P` toggles section placement, `J` jump, `D` drop, `M` mantle, and `Escape` cancels.
9. The contact frame is aligned to the hit surface normal, with a stable up-axis fallback and a 5 cm default outward offset.

The current prototype uses the visibility collision channel and complex traces. Geometry must have collision that blocks that channel.

## Current scope and limitations

- Straight paths only; curved interpolation is not implemented.
- The editor mode currently provides node/edge visualization, color-coded clickable handles, and click-to-place interaction.
- Drag-to-move handles, a true ghost character preview, skeletal preview, and runtime graph compilation are subsequent milestones.
- The route actor validates graph references but does not yet perform gameplay reachability analysis.
- This branch has not been compiled against an installed Unreal Engine in this environment. Compile and smoke-test in UE 5.8 before merging.
