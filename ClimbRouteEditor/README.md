# Climbing Route Authoring v2

This branch contains the authoring foundation and the first route-driven runtime climbing pass for the new climbing system. The new runtime is `UClimbRuntimeComponent`; it consumes `AClimbRouteActor` data and does not depend on the old spline-based route graph.

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

## Runtime climbing prototype

The new `UClimbRuntimeComponent` provides route discovery, closest-node attachment, surface-facing orientation, explicit graph navigation, section movement, and initial jump/mantle/drop link execution. It exposes runtime state and Blueprint-callable methods for clean future animation and combat coordination.

See [RuntimeClimbing.md](RuntimeClimbing.md) for setup instructions, input wiring, smoke tests, and current limitations.

## Current scope and limitations

- Straight section movement currently interpolates between section endpoint nodes; intermediate path points are not yet followed.
- Jump and mantle links use a simple sine arc. Drop and surface-transfer links use direct interpolation in this first pass.
- Movement currently sets the character movement mode to `MOVE_None` and moves the actor directly. Collision-aware movement, animation/root-motion matching, and robust corner transitions are future work.
- Dynamic routes spawned after BeginPlay require calling `RefreshAvailableRoutes`.
- The editor route actor validates graph references but does not yet perform gameplay reachability analysis.
- This branch has not been compiled against an installed Unreal Engine in this environment. Compile and smoke-test in UE 5.8 before merging.
