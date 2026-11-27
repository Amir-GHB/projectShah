# Native Horse Riding System

This is the first single-player C++ riding implementation. It intentionally has no replication, server/client branching, or first-person mode.

## Runtime architecture

- `ACombatCharacterBase` owns a reusable `URidingComponent`.
- `AHorseCharacter` is the possessed horse pawn. It owns horse movement and the horse camera.
- The rider remains an actor attached to the horse skeletal mesh at `RiderAttachSocketName`.
- The player controller possesses the horse while mounted. The rider's `UCombatComponent`, weapon actors, damage components, and targeting component remain on the rider.
- Horse input forwards attack, weapon switch, guard, parry, and target-lock actions to the rider's existing combat components.
- `UCombatAnimInstance` exposes the rider's `RidingState` and `MountDirection` enums.
- `UHorseAnimInstance` calculates horse locomotion, turning/body values, side-stack state, and component-space foot IK targets.

## Rider Blueprint setup

Use a Blueprint derived from `ACombatCharacterBase`.

Assign the existing Enhanced Input assets:
- `DefaultMappingContext`
- `MoveAction`
- `LookAction`
- `AttackAction`
- `GuardAction`
- `ParryAction`
- `NextWeaponAction`
- `TargetLockAction`
- `MountAction`
- `SprintAction`

The mapping context must contain the mount and sprint actions. The riding component copies the rider's input action references to the horse immediately before possession, so those actions do not need to be assigned again in the horse Blueprint.

`MountAction` is also used as the horse's dismount action.

Configure the `RidingComponent`:
- Mount and dismount montages for Left, Right, and Rear/Center.
- Mount search radius and transform transition duration.
- Camera blend duration.
- Preferred dismount direction.

Mount/dismount montages should be authored in-place. The riding component drives the actor transform during these transitions. Set `MountTransitionDuration` to match the authored animation timing.

The rider Animation Blueprint should derive from `UCombatAnimInstance`. Use:
- `RidingState`: Unmounted, Mounting, Mounted, Dismounting, Falling.
- `MountDirection`: None, Left, Right, Center.

Use the enum values for state-machine transitions instead of maintaining several independent mount/dismount booleans.

## Horse Blueprint setup

Create a Blueprint derived from `AHorseCharacter`.

1. Assign the horse skeletal mesh and its normal locomotion Animation Blueprint.
2. Add a skeletal-mesh socket named `Rider_Socket` (or set `RiderAttachSocketName` to your socket). Place and rotate it at the rider's final saddle position.
3. Set the horse camera boom and camera framing.
4. Tune `WalkSpeed`, `SprintSpeed`, `ReverseSpeed`, and `TurnRate`.
5. Tune the left/right/rear fallback mount offsets if the rider socket is unavailable, and configure left/right/rear dismount offsets.

When the rider socket exists, its world transform is the authoritative mount destination. This avoids a snap when the rider attaches after the transition.

The horse Animation Blueprint should derive from `UHorseAnimInstance`. Available data includes:
- `GroundSpeed`, signed `ForwardSpeed`, `Acceleration`, `bIsMoving`, `bIsAccelerating`, and `bIsSprinting`.
- `TurnAlpha` and `bSideStack`.
- `ChestRotation`, `SpineRotation`, `HipRotation`, `LeftLegRotation`, `RightLegRotation`, and `TiltRotation`.
- `FrontLeftIK`, `FrontRightIK`, `BackLeftIK`, `BackRightIK`, `FrontLegIKAlpha`, and `BackLegIKAlpha`.

The four IK vectors are component-space effector locations. Set the corresponding Two Bone IK nodes to Component Space. Configure the four foot-bone names to match the horse skeleton. If the configured pair of foot bones is missing, its IK alpha remains disabled.

## Mounted combat setup

The existing combat component and weapon actors are reused. Mounted attacks still play on the rider mesh, and weapon traces/damage continue through the existing combat pipeline.

For each weapon's `FWeaponConfig`, configure:
- `MountedAttackComboMontage` when using a single combo montage with sections.
- `MountedIndividualAttackMontages` when using one montage per combo step.

If a mounted attack slot is left empty, the combat component falls back to the ordinary on-foot montage so the weapon remains usable during setup.

For the bow, configure `MountedDrawMontage` and `MountedReleaseMontage` on the bow weapon Blueprint.

For mounted hit and death presentation, configure `MountedHitReactionMontage` and `MountedDeathMontage` on the rider's `DamageReactionComponent`. On death while mounted, the rider detaches, player control returns to the rider for the death sequence, and the horse is left behind.

Attack positioning/Motion Warping is deliberately disabled for mounted attacks because the rider is attached to the horse and should not be independently repositioned by the foot-combat positioning component.

## Current scope and limitations

- Mount selection is currently the nearest mountable horse inside `MountSearchRadius`. The original overlap hint actors are not ported in this first pass.
- Mounting from Left, Right, and Rear/Center is selected from the rider's position relative to the horse.
- Dismount direction is configurable through `PreferredDismountDirection`.
- Camera input and movement input are paused during the transform transition and camera handoff.
- This first pass does not implement nonfatal knock-off/fall recovery, horse death, mounted guard-specific montages, or advanced leaning/hiding animations.
- No Unreal build or runtime test has been run from this GitHub editing session. Compile and test in the UE 5.8 project before relying on it in a demo.
