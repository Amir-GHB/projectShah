# Route-Driven Runtime Climbing: First Playable Pass

The new runtime component is `UClimbRuntimeComponent` in `ClimbSystem`. It consumes only
`AClimbRouteActor`, `FClimbRouteNode`, `FClimbRouteSection`, and
`FClimbRouteTraversalLink`. It does not read the legacy spline-based `AClimbRoute`
or the legacy `UClimbingComponent`.

## What is implemented

- Finds the closest authored node within a configurable attachment radius.
- Places the character capsule outside the authored surface frame and turns the character toward the surface.
- Uses explicit sections and traversal links as the navigation graph. It does not infer connections by proximity.
- Moves along authored sections in either direction when `bBidirectional` is enabled (the default).
- Exposes `EClimbMovementDirection` to Blueprint for distinct Up, Down, Left, Right, Jump, Mantle, and Exit animation selection.
- Treats downward section travel as a ledge release, accelerating fall, and lower-node catch. The state becomes `DroppingToCatch`, distinct from ordinary section movement.
- Executes authored jump and mantle links with a simple configurable sine arc.
- Treats a node of type `Exit` as a top-out point and restores the character's previous movement mode after reaching it.
- Builds muted-gold path connections and short world-vertical node ticks using instanced static-mesh geometry at runtime. These are ordinary scene components, not development-only debug lines.

This is a first playable foundation, not a finished production traversal system. It currently moves the character actor directly while character movement is set to `MOVE_None`; collision-aware path following, animation montage/root-motion matching, capsule/landing validation, and robust transitions around corners remain future work.

## Fastest test setup: dedicated test character

The branch now includes `AClimbTestCharacter`, a minimal test character with `UClimbRuntimeComponent`, ordinary ground movement, and a third-person follow camera.

- **W / S**: move forward / backward on the ground; move up / down along the route while climbing.
- **A / D**: strafe left / right on the ground; move left / right across the route while climbing.
- **E**: attach to the closest usable authored node, or detach if already climbing.
- **Mouse**: rotate the follow camera, provided the project has the usual `Turn` and `LookUp` axis mappings.

1. Compile the project in Unreal Engine 5.8.
2. Create a Blueprint child of `AClimbTestCharacter` (recommended) to assign a skeletal mesh and animation Blueprint. The camera boom, follow camera, and basic movement are already created in C++.
3. In the test map, set this Blueprint as the Default Pawn Class in the active GameMode, or place it and configure the Player Controller to possess it.
4. Build a route as described below and press Play.

The C++ class intentionally does not hard-code a project-specific mesh or animation setup. Its direct keyboard bindings are a temporary test harness; production input should use the project's chosen input mapping layer. If mouse look does not work in a custom project, add the conventional `Turn` and `LookUp` axis mappings in Project Settings > Input.

## Using the runtime component on another character

1. Add the **Climb Runtime Component** (`UClimbRuntimeComponent`) to the character.
2. On climbing input, call `StartClimbing`; call `StopClimbing` to detach.
3. Feed surface-relative movement into `SetClimbInput`. X means left/right across the surface; Y means up/down.
4. Ensure the character is the possessed pawn and call `RefreshAvailableRoutes` if routes are spawned after BeginPlay.

The component does not bind project-specific input actions itself. Keeping input mapping in the character/controller avoids coupling climbing to one project's Enhanced Input assets.

## Author a bidirectional route and top-out exit

1. Select the route actor and activate **Climb Route** mode.
2. Click a node to select it. Four round action buttons appear around the selected node.
3. Choose the **blue** button, then move the mouse over the next surface point. A ghost point and preview connection follow the cursor; click to create the node and section.
4. Because sections are bidirectional by default, the same blue section can be traversed up or down. Descending sets runtime state to `DroppingToCatch` and direction to `Down`, allowing a separate fall/catch animation.
5. At the top of the climb, select the final wall node and click the **teal exit** button. Move the cursor onto the horizontal top surface. The preview is red on a vertical wall and teal on an accepted upward-facing surface.
6. Click the top surface to create an `Exit` node and a mantle link from the selected source. Reaching the exit places the capsule upright on the top surface and restores normal character movement.
7. The exit currently checks surface orientation only. Capsule fit, ledge height, walkable landing area, and clearance validation are intentionally left for the next pass.

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
- Exit placement accepts only surfaces whose normal has a world-Z component of at least 0.65. It does not yet check ledge height, capsule size, or landing clearance.
- Runtime route lines are built at BeginPlay. If you modify route data dynamically during play, the visualization needs rebuilding as well as refreshing route discovery.
- Section movement is straight interpolation between node anchors. Intermediate path points are not yet followed by the runtime.
- Traversal arcs are a prototype, not animation-matched movement.
- Movement is not yet swept against collision while following a route.
- Downward catch movement has a procedural trajectory and animation direction/state, but is not yet synchronized to a production animation montage or root motion.
- There is no automatic route connection, reachability validation, or automatic route discovery based on nearby nodes.
