# Route-Driven Runtime Climbing: First Playable Pass

The new runtime component is `UClimbRuntimeComponent` in `ClimbSystem`. It consumes only
`AClimbRouteActor`, `FClimbRouteNode`, `FClimbRouteSection`, and
`FClimbRouteTraversalLink`. It does not read the legacy spline-based `AClimbRoute`
or the legacy `UClimbingComponent`.

## What is implemented

- Finds the closest authored node within a configurable attachment radius.
- Places the character capsule outside the authored surface frame and turns the character toward the surface.
- Uses explicit sections and traversal links as the navigation graph. It does not infer connections by proximity.
- Moves between section endpoints at `ClimbSpeed`.
- Executes authored jump and mantle links with a simple configurable sine arc; drop and surface-transfer links use direct interpolation in this first pass.
- Exposes a small Blueprint-facing API and runtime state for later animation/combat coordination.

This is a first playable foundation, not a production traversal system. It currently moves the character actor directly while character movement is set to `MOVE_None`; collision-aware path following, animation montages/root motion, ledge validation, and robust transitions around corners are future work.

## Add it to the test character

1. Compile the project in Unreal Engine 5.8.
2. Open the Character Blueprint used for testing.
3. Add the **Climb Runtime Component** (`UClimbRuntimeComponent`).
4. On the climbing input pressed event, call `StartClimbing`.
5. While climbing, feed the existing 2D movement input into `SetClimbInput`. X means left/right across the surface; Y means up/down.
6. On the movement input's Enhanced Input `Completed` and `Canceled` events, call `SetClimbInput(0, 0)`. This is important: the current segment finishes after input release, but zero input prevents starting another segment at the next node.
7. On the climbing input released/toggle-off event, call `StopClimbing`.
8. Ensure the Character Blueprint is actually the possessed pawn in the test map.

The component does not bind project-specific input actions itself. Keeping input mapping in the character/controller avoids coupling climbing to one project's Enhanced Input assets.

## Build a minimal test route

1. Place one `AClimbRouteActor` against a wall with collision.
2. Create two nodes on the same wall.
3. Add a blue straight section between them. Make the node positions far enough apart to see movement.
4. Ensure both nodes have `Can Start Traversal` and `Can End Traversal` enabled.
5. Place the character near the first node, within `MaxAttachDistance`.
6. Press the climbing input. The character should snap to the nearest usable node and face the wall.
7. Press the direction that points from the first node toward the second. The character should move along the section and settle at the destination node.
8. Release movement during travel. The current segment should finish; no next segment starts until directional input is supplied again.
9. Press the climb input to stop and confirm the previous movement mode is restored.

## Debugging

- Set `bDrawRuntimeDebug` to see the active segment/traversal line.
- Check the Output Log for an attachment message or the verbose no-node-within-range message.
- If attachment fails, move closer or increase `MaxAttachDistance`.
- If attachment works but movement does not, verify the two nodes have an explicit section/link and that the input direction points toward its destination.
- For routes spawned after BeginPlay, call `RefreshAvailableRoutes` before attempting attachment.

## Known first-pass limitations

- The route graph is scanned at BeginPlay; dynamic routes require `RefreshAvailableRoutes`.
- Node contact frames are expected to use local X as the outward surface normal and local Z as preferred up.
- Section movement is straight interpolation between node anchors. Intermediate path points are not yet followed by the runtime.
- Traversal arcs are a prototype, not animation-matched movement.
- Movement is not yet swept against collision while following a route.
- There is no automatic route connection, reachability validation, or automatic route discovery based on nearby nodes.
