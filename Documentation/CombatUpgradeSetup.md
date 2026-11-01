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

## 5. Training dummy

Create a Blueprint child of `ACombatDamageDummy` and assign a **Skeletal Mesh** and compatible Animation Blueprint to its inherited Character Mesh component.

- Configure the capsule size/position to fit the assigned skeletal mesh. The capsule is the Pawn collision root; the skeletal mesh handles hit traces.
- Configure Max Health, Armor, and Max Poise on its `DamageHandlerComponent`.
- Configure `HitReactions`, `DeathReactions`, and `DeathMontage` on its `DamageReactionComponent`. Assign montages compatible with the dummy mesh's skeleton and ensure the AnimBP has a montage slot (normally `DefaultSlot`).
- The actor includes a world-space health bar using `UCombatHealthBarWidget`.
- A Widget Blueprint can derive from `UCombatHealthBarWidget` and provide a styled ProgressBar named `HealthBar`; assign that class to `HealthBarWidgetClass`.
- The dummy's CharacterMovement starts disabled so it remains stationary. Damage reactions and death montages still play through its skeletal mesh AnimInstance.
- Call `ResetDummy` from Blueprint to restore health/poise, clear the death latch, stop the current reaction montage, and re-enable walking movement for another test.

The dummy uses the same damage handler and reaction component as combat characters, so it can test normal damage, criticals, hit regions, status effects, hit reactions, and death reactions.

## 6. Semi-lock targeting

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

## 7. Suggested test order

1. Create the dummy Blueprint, assign a skeletal mesh and AnimBP, then configure hit/death montages.
2. Place the dummy in the level and verify ordinary melee hits reduce its health and play a hit reaction.
3. Set one combo step's base damage override and confirm that step differs from the others.
4. Set critical chance to 1.0 and verify the expected critical damage; then set it to 0.0.
5. Apply a short poison effect and verify tick count, interval, and total health loss.
6. Try each stacking policy by applying the same effect repeatedly.
7. Map a head bone and a leg bone, then verify region multipliers and corresponding reaction filters.
8. Assign a death montage or death reaction, lower dummy health, and verify the death montage plays and subsequent hits are ignored.
9. Call `ResetDummy` and verify the dummy can be damaged and react again.
10. Place several damageable actors around the player. Confirm the most view-centered valid target is selected.
11. Start and end an attack repeatedly. Confirm the character only turns during the attack, stops focusing afterward, and never focuses on a target outside MaxFocusRange.
12. Assign a TargetLockAction and confirm manual preference persists between attacks without rotating the idle character.

## Verification note

The source changes are committed to this feature branch, but they have not been compiled or run in Unreal Editor from this environment. Treat the first local build as the compile verification step before gameplay testing.
