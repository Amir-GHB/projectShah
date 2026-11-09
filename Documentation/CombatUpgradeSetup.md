# Combat Upgrade: Setup and Test Guide

This branch adds damage resolution, status effects, damage reactions, a training dummy, and attack-scoped semi-lock targeting to the existing combat code. It does not modify the separate `Combat-system` repository.

## 1. Per-attack damage profiles

Open the weapon Blueprint that derives from `AWeaponBase` and edit its `WeaponConfig`.

- `BaseDamage`, `CriticalChance`, `CriticalDamageMultiplier`, and `DamageOverTimeEffects` are weapon-wide defaults.
- `AttackDamageConfigs` is indexed by combo step.
- In a single combo montage, index 0 corresponds to section 0, index 1 to section 1, and so on.
- In individual montage mode, the array index matches the individual attack montage index.
- Enable `bOverrideBaseDamage` to replace the weapon's base damage for that step.
- `DamageMultiplier` always scales that step's base damage.
- Enable `bOverrideCriticalSettings` to use that step's critical chance and critical multiplier instead of weapon defaults.
- Enable `bOverrideDamageOverTimeEffects` to use that step's effect list, including an intentionally empty list.

Critical chance is a probability from 0 to 1. The target's `UDamageHandlerComponent` rolls the chance and applies the critical multiplier before armor mitigation.

## 2. Damage-over-time effects

Each `FDamageOverTimeEffect` has its own:

- `EffectId`: use a unique ID when you need multiple distinct effects of the same family.
- `Type`: Poison, Burning, Bleeding, or Custom.
- `DamagePerTick`, `Duration`, and `TickInterval`.
- `DamageCategory`.
- `StackingPolicy`: Ignore, Refresh, or Stack.
- `MaxStacks`: used by Stack policy.

Example: 4 damage every 1 second for 5 seconds produces five ticks, for 20 raw damage before armor. Periodic ticks do not roll critical hits or trigger ordinary hit-reaction montages.

The handler only enables component ticking while at least one effect is active.

## 3. Hit locations and directions

Each target's `UDamageHandlerComponent` has configurable multipliers for Head, Torso, Arms, and Legs.

For skeletal meshes, populate `BoneHitLocationMap` with the exact bone names used by the target skeleton. This is the reliable way to distinguish arms from torso. If a bone is not mapped, the handler estimates Head, Torso, or Legs from the hit component's vertical bounds.

Incoming direction is classified relative to the target's local forward/right axes as Front, Back, Left, or Right. Melee and projectile paths supply the attack direction.

## 4. Hit and death reactions

`ACombatCharacterBase` owns a `UDamageReactionComponent`.

- Add entries to `HitReactions` and/or `DeathReactions`.
- Each entry can optionally filter by weapon type, hit location, and incoming direction.
- The most specific matching entry wins. Entries with no filters act as fallbacks.
- `DeathMontage` is the final fallback if no death-reaction entry matches.

Reaction montages are assigned per character Blueprint because the actual animation assets and skeletons are character-specific.

## 5. Last-enemy finishers (initial implementation)

Finishers are coordinated by the attacker's existing `UDamageReactionComponent`; no additional actor component is required. The attacker's `UCombatComponent` owns the list of enemies registered for the current encounter. The paired finisher can target any character with the shared damage/reaction components, including native AI enemies.

### Player setup

On the player Blueprint:

- Add entries to `DamageReactionComponent.FinisherMontages`.
- Each entry pairs an `AttackerMontage` and a `VictimMontage` for one `WeaponType`.
- Set `Direction` to Front, Back, Left, or Right to restrict the pair to that incoming hit direction. Set it to All for a universal fallback.
- Direction comes from the killing blow's existing target-relative `HitDirection`. Left and Right are from the victim's perspective.
- Add multiple entries for the same weapon and direction to get random variations. Exact directional entries always take priority over All entries.
- Cinematic camera is disabled by default for every pair. Enable `bUseCinematicCamera` only on entries that need a custom shot.
- Configure `CameraShot.RelativeOffset` in the victim's local axes (X forward, Y right, Z up), choose whether to look at the midpoint, attacker, or victim, and tune FOV and blend times. Enable `bFollowCharacters` to keep the camera framed as the paired animations move.

### Registering encounter enemies

Native `AEnemyCharacter` actors now register themselves with the first player's `CombatComponent` during BeginPlay. The enemy AI also registers itself when it acquires the player by sight, covering cases where the player pawn is spawned after the enemy. They unregister when the enemy ends play or dies.

Non-AI test actors such as `ACombatDamageDummy` are not automatically treated as encounter enemies; register those manually from Blueprint with `RegisterEncounterEnemy` if you want to test last-enemy finishers against them.

Every finisher victim must have `UDamageHandlerComponent` and `UDamageReactionComponent`. The last living registered victim is eligible for a paired finisher. The attacker must have a `UCombatComponent` and matching weapon/direction montage pair.

### Current rules and limitations

- Only a direct melee killing blow from an attacker with a `UCombatComponent` can trigger a finisher. Bow/projectile and damage-over-time kills use the normal death reaction. The attacker does not need to be player-possessed; a player controller is only needed for player input locking and optional cinematic camera control.
- The victim must be registered in the player's encounter list and be the last living registered enemy.
- A valid montage pair must exist for the killing weapon type. If not, the ordinary death reaction plays.
- The victim's ordinary death montage is suppressed only when both finisher montages successfully start.
- The gameplay camera remains active unless the selected pair explicitly enables its cinematic shot. An enabled shot uses its own relative offset, look-at target, FOV, blend times, and optional character-follow behavior. If either montage is interrupted, the other is stopped and camera/control state is restored.
- Motion Warping and automatic attacker/victim positioning are not included yet. For this first test, position the characters close enough for the authored paired animations to line up, and make sure both montages are compatible with their respective skeletons and AnimBP montage slots.
- The attacker enters the temporary `Finishing` combat state, which blocks normal attack and weapon-switch input until the paired montages finish.

## 6. Training dummy

Create a Blueprint child of `ACombatDamageDummy` and assign a **Skeletal Mesh** and compatible Animation Blueprint to its inherited Character Mesh component.

- Configure the capsule size/position to fit the assigned skeletal mesh. The capsule is the Pawn collision root; the skeletal mesh handles hit traces.
- Configure Max Health, Armor, and Max Poise on its `DamageHandlerComponent`.
- Configure `HitReactions`, `DeathReactions`, and `DeathMontage` on its `DamageReactionComponent`. Assign montages compatible with the dummy mesh's skeleton and ensure the AnimBP has a montage slot (normally `DefaultSlot`).
- The actor includes a world-space health bar using `UCombatHealthBarWidget`.
- A Widget Blueprint can derive from `UCombatHealthBarWidget` and provide a styled ProgressBar named `HealthBar`; assign that class to `HealthBarWidgetClass`.
- The dummy's CharacterMovement starts disabled so it remains stationary. Damage reactions and death montages still play through its skeletal mesh AnimInstance.
- Call `ResetDummy` from Blueprint to restore health/poise, clear the death latch, stop the current reaction montage, and re-enable walking movement for another test.

The dummy uses the same damage handler and reaction component as combat characters, so it can test normal damage, criticals, hit regions, status effects, hit reactions, and death reactions.

## 7. Semi-lock targeting

`ACombatCharacterBase` owns a `UCombatTargetingComponent`.

- The component only ticks while a melee attack montage is active.
- It scores valid nearby actors using view alignment, screen-center alignment, distance, and a small target-retention bonus.
- Valid targets can use either the new `UDamageHandlerComponent` or the repository's existing `ICombatDamageable` interface, and must be within `MaxFocusRange`. Component-backed targets are rejected when dead. The legacy Variant_Combat actors retain their existing damage/health implementation; the new critical, hit-region, and status-effect calculations apply to targets using `UDamageHandlerComponent`.
- Visibility is checked using the Visibility channel when `bRequireLineOfSight` is enabled.
- The character smoothly turns toward the selected target during the attack.
- A short movement-input correction is attempted at close-to-melee distances, once per attack, while grounded and moving slowly.
- Focus ends when the attack ends or is interrupted. The character's previous `bOrientRotationToMovement` setting is restored.
- A manually selected target is remembered as a preference, but does not cause rotation or movement while idle.

Assign an Enhanced Input Action to `TargetLockAction` on the character Blueprint and bind it in the Input Mapping Context to enable the manual target toggle. Leaving it unassigned keeps automatic target selection only.

Default tuning values are intentionally conservative and exposed on the component: maximum focus range 700 cm, correction window 160-300 cm, correction duration 0.12 seconds.

## 8. Suggested test order

1. Create the dummy Blueprint, assign a skeletal mesh and AnimBP, then configure hit/death montages.
2. Configure three paired finisher montage entries for the player's weapon on its DamageReactionComponent.
3. Register two or more test enemies with the player's CombatComponent, then kill them one by one. Verify only the final registered living enemy triggers a paired finisher.
4. Verify an unregistered enemy, a bow kill, a DoT kill, or a missing weapon montage pair uses the normal death reaction.
5. Set one combo step's base damage override and confirm that step differs from the others.
6. Set critical chance to 1.0 and verify the expected critical damage; then set it to 0.0.
7. Apply a short poison effect and verify tick count, interval, and total health loss.
8. Try each stacking policy by applying the same effect repeatedly.
9. Map a head bone and a leg bone, then verify region multipliers and corresponding reaction filters.
10. Assign a death montage or death reaction, lower dummy health, and verify the death montage plays and subsequent hits are ignored.
11. Call ResetDummy and verify the dummy can be damaged and react again.
12. Place several damageable actors around the player. Confirm the most view-centered valid target is selected.
13. Start and end an attack repeatedly. Confirm the character only turns during the attack, stops focusing afterward, and never focuses on a target outside MaxFocusRange.
14. Assign a TargetLockAction and confirm manual preference persists between attacks without rotating the idle character.

## Verification note

The source changes are committed to this feature branch, but they have not been compiled or run in Unreal Editor from this environment. Treat the first local build as the compile verification step before gameplay testing.


## 7. Weapon hit and swing feedback

Configure these fields in each weapon Blueprint's `WeaponConfig`:

- `HitNiagara` and `HitSound` play at the confirmed impact point, with Niagara aligned to the impact normal.
- `SwingNiagara` is spawned attached to the weapon mesh, so it follows the moving blade instead of remaining at a world-space point.
- `SwingEffectSocketName` optionally anchors the trail to a weapon-mesh socket; `SwingEffectRelativeTransform` offsets it from that socket.
- `SwingSound` remains a one-shot sound at the configured swing-effect transform.
- Add `Weapon Swing Effects` to attack animation frames where the blade passes through the swing. Its notify properties independently enable sound and Niagara.
- Add the new `Weapon Hit Effects` notify wherever an authored impact VFX is needed. It reads the character's currently equipped weapon and plays that weapon's configured `HitNiagara` and `HitSound` at the notify's optional weapon socket. Actual confirmed trace hits continue to spawn impact effects automatically.

## 8. Parry

Trigger `BeginParryWindow` from the player's parry input, or add `Parry Window` notify state over the active frames of the defensive animation (the notify state automatically uses its authored duration). Set the default defender and attacker-parried montages on the character's `DamageReactionComponent`. A defender's equipped weapon-specific parry montages override those defaults. The defender's weapon `ParryDamage` is applied to the attacker when a weapon trace intersects the active parry window. A successful parry now explicitly cancels the attacker's active combat transaction before playing the parried reaction, closing its weapon trace and invalidating queued combo transitions.

## 9. Death ragdoll

`DamageReactionComponent.bRagdollOnDeath` enables shared ragdoll behavior for player and enemy characters. The skeletal mesh hands off to physics at the death montage's final blend-out rather than waiting for its end delegate, avoiding the brief return to the AnimBP's standing pose. Ensure the skeleton has a valid Physics Asset and the project has a Ragdoll collision profile. The transition still depends on a valid death montage and compatible collision/physics setup.
