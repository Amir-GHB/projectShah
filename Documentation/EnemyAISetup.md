# Enemy Character and Basic AI Setup

This branch adds a native enemy foundation using the existing combat and damage systems.

## Included native classes

- `AEnemyCharacter`: character base with `UCombatComponent`, `UDamageHandlerComponent`, and `UDamageReactionComponent`.
- `AEnemyAIController`: native patrol, sight/hearing perception, investigation, pursuit, and basic weapon attacks.
- Unreal AI Perception is configured in C++; no Behavior Tree or Blackboard asset is required for this first implementation.

## Minimal Blueprint setup

Create a child Blueprint of `AEnemyCharacter` and configure:

1. Skeletal mesh and Animation Blueprint.
2. `CombatComponent.WeaponClasses` and `DefaultWeaponType` for the enemy's weapon. The existing combat component spawns and equips the configured weapon.
3. Damage/reaction values and optional hit/death reaction montages.
4. Optionally place patrol point actors and assign them to `PatrolPoints`. If the array is empty, the enemy chooses random reachable destinations within `RandomPatrolRadius` when `bUseRandomPatrolWhenNoPoints` is enabled.
5. Adjust sight, hearing, attack range, patrol wait, and investigation values if needed.

The enemy automatically possesses `AEnemyAIController` when placed or spawned.

## Current behavior

- Patrols the assigned points in array order and loops.
- Sight detects player-controlled Pawns inside the configured sight cone.
- Hearing records a noise location and investigates it for a limited duration.
- Losing sight of a target moves the enemy to the last seen location before resuming patrol.
- Pursues a visible player and calls the enemy's virtual combat action inside `AttackRange`. `AMagicEnemyCharacter` instead launches its configured projectile Blueprint toward the target.
- Stops its AI update loop when its damage handler reports death.

For sound detection, gameplay systems should report noise with Unreal's `UAISense_Hearing::ReportNoiseEvent` (or an equivalent Pawn noise event). Merely playing an audio asset does not automatically create an AI hearing stimulus.

## Combat component decision

The existing `UCombatComponent` is reused because its weapon spawning, equip flow, montage handling, combo logic, and weapon trace integration are character-agnostic enough for an AI Pawn. The AI invokes `HandleAttackInput()` when its target is in range and the component is idle. Player input and camera targeting are not added to the enemy.

Configure the enemy's weapon classes in its child Blueprint. Do not add `UCombatTargetingComponent`; it is player-camera/attack-focus oriented.

## Blackboard and Behavior Tree

Unreal supports both Blackboard and Behavior Tree workflows from C++: native controllers can create/configure Blackboard data and start Behavior Trees, while behavior tasks/services/decorators can be implemented in C++. This initial controller deliberately uses a small native state loop and AI Perception directly, avoiding required editor-authored Blackboard/Behavior Tree assets. They can be introduced later if the behavior grows beyond this compact state machine.

## Navigation requirements

- Add a NavMeshBoundsVolume covering patrol and combat areas.
- Build navigation data.
- Ensure patrol points and enemy spawn positions lie on navigable space.
- Ensure the enemy capsule and mesh use appropriate collision settings.

## Scope and limitations

This is a first-pass foundation, not a full tactical AI:
- Targets player-controlled Pawns only; faction/team filtering is not yet integrated.
- One target at a time; no squad coordination, alert propagation, cover, or ranged combat tactics.
- The combat component needs valid weapon classes and compatible animation montages for attacks to occur.
- Hearing requires explicit gameplay noise events.
- Code has not been compiled or run in Unreal Editor as part of this repository edit.


## Magic enemy

Create a child Blueprint of `AMagicEnemyCharacter`, assign `MagicProjectileClass`, and optionally configure `ProjectileSpawnSocket`, `ProjectileSpawnOffset`, `MagicAttackCooldown`, and `AttackRange`. The projectile Blueprint should own its movement, collision, damage, and visual configuration (for example, a ProjectileMovement component with an Initial Speed). The native caster aims its spawn rotation toward the target's upper body.

## Random patrol

With no patrol points assigned, `bUseRandomPatrolWhenNoPoints` makes the controller request a random reachable NavMesh point inside `RandomPatrolRadius`, wait for `PatrolWaitTime`, and choose another. A NavMeshBoundsVolume is still required.
