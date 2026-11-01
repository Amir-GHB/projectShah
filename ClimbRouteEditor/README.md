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
5. Click a node to select it. The selected node is drawn larger and yellow, and a radial menu appears around it with four circular actions:
   - **Blue, connected points:** create a bidirectional climbing section, usable both up and down.
   - **Green, up arrow:** create a mantle link.
   - **Orange, diagonal arrow:** create a jump link.
   - **Teal, exit arrow:** create an exit on an upward-facing horizontal surface.
6. Click an action button, then move the mouse over collision geometry to preview a ghost point and connecting line. Click to commit; press `Escape` to cancel. Exit placement shows red when the cursor is over an invalid vertical or underside surface.
7. Downward travel along a bidirectional section is automatic. It is classified separately for animation and uses a release, accelerating fall, and catch motion.
8. Keyboard alternatives: `N` toggles isolated-node placement, `P` toggles section placement, `J` jump, `D` explicit drop link, `M` mantle, and `Escape` cancels.
9. The contact frame is aligned to the hit surface normal, with a stable up-axis fallback and a 5 cm default outward offset.

The current prototype uses the visibility collision channel and complex traces. Geometry must have collision that blocks that channel.

## Runtime climbing prototype

The new `UClimbRuntimeComponent` provides route discovery, closest-node attachment, surface-facing orientation, explicit graph navigation, bidirectional section movement, jump/mantle execution, and a ledge-release/fall/catch descent. Blueprint can read both the runtime state and `EClimbMovementDirection` (Up, Down, Left, Right, Jump, Mantle, Exit) to select animations. Exit nodes return the character to normal movement. `AClimbRouteActor` also builds muted gold path lines and node ticks from instanced mesh geometry at runtime, so the visualization is not dependent on development-only debug lines.

See [RuntimeClimbing.md](RuntimeClimbing.md) for setup instructions, input wiring, smoke tests, and current limitations.

## Current scope and limitations

- Straight section movement currently interpolates between section endpoint nodes; intermediate path points are not yet followed.
- Jump and mantle links use a simple sine arc; the down-to-catch movement is a first-pass trajectory and will need animation/root-motion matching for production polish.
- Movement currently sets the character movement mode to `MOVE_None` and moves the actor directly. Collision-aware movement, animation/root-motion matching, and robust corner transitions are future work.
- Dynamic routes spawned after BeginPlay require calling `RefreshAvailableRoutes`.
- Exit nodes currently validate only the upward-facing surface normal. Height, capsule clearance, landing space, and reachability checks remain future work.
- Runtime path visualization is made from thin engine cylinder meshes with a configurable muted-gold material; it has configurable line thickness and endpoint gaps.
- This branch has not been compiled against an installed Unreal Engine in this environment. Compile and smoke-test in UE 5.8 before merging.
