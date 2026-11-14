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

Critical chance is a probability from 0 to 1. The incoming-hit resolver rolls it once before guard/parry resolution; `UDamageHandlerComponent` respects the resolved result and applies the critical multiplier before armor mitigation.

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


## 9. Weapon hit and swing feedback

Configure these fields in each weapon Blueprint's `WeaponConfig`:

- `HitNiagara` and `HitSound` play at the confirmed impact point, with Niagara aligned to the impact normal.
- `SwingNiagara` is spawned attached to the weapon mesh, so it follows the moving blade instead of remaining at a world-space point.
- `SwingEffectSocketName` optionally anchors the trail to a weapon-mesh socket; `SwingEffectRelativeTransform` offsets it from that socket.
- `SwingSound` remains a one-shot sound at the configured swing-effect transform.
- Add `Weapon Swing Effects` to attack animation frames where the blade passes through the swing. Its notify properties independently enable sound and Niagara.
- Add the new `Weapon Hit Effects` notify wherever an authored impact VFX is needed. It reads the character's currently equipped weapon and plays that weapon's configured `HitNiagara` and `HitSound` at the notify's optional weapon socket. Actual confirmed trace hits continue to spawn impact effects automatically.

## 10. Guard and parry

Guard and parry animations are weapon-owned. Configure `BeginGuardMontage`, `BreakGuardMontage`, `DefenderParryMontage`, and `AttackerParriedMontage` in each weapon's `WeaponConfig`. Tune `ParryDamage` (20) and `ParryPoiseDamage` (100) there as well. `ParryPoiseDamage` is enough to break the default 100-point poise pool, staggering the attacker. Do not assign shared guard/parry montage defaults on `DamageReactionComponent`; those legacy defaults have been removed.

On the character Blueprint, assign two Enhanced Input actions to `GuardAction` and `ParryAction`:

- Guard is held: Started enters guard; Completed/Canceled releases it.
- Parry is pressed while guarding: Started calls `BeginParryAttempt`.
- Guard/parry state is exposed through `FCombatAnimationState.DefenseState`. Use `Guarding`, `Parrying`, and `GuardBroken` in the AnimBP to drive the equipped weapon's guard blend space and transitions.
- Add the `Parry Window` notify state to the attacker's attack montage for explicit parryable frames. If a montage has no such notify state, the active melee trace window is the fallback.
- A successful parry invalidates and stops the attacker's active attack transaction before applying the reward. The ordinary hit VFX and normal hit reaction are suppressed; the attacker's weapon-specific `AttackerParriedMontage` is then played.
- A perfect parry grants the defender a short riposte window. A melee attack during that window receives `RiposteDamageMultiplier`; attacks from guard outside that window remain blocked.

`ParryWindowDuration = 0` means the defender parry montage length. A nonzero value overrides it. A whiff or cancelled attempt applies `ParryCooldown`; a successful parry does not. Releasing guard, taking a valid hit, guard break, death, finisher, and weapon swap all clear the active attempt.

## 11. Death ragdoll

`DamageReactionComponent.bRagdollOnDeath` enables shared ragdoll behavior for player and enemy characters. The skeletal mesh hands off to physics at the death montage's final blend-out rather than waiting for its end delegate, avoiding the brief return to the AnimBP's standing pose. Ensure the skeleton has a valid Physics Asset and the project has a Ragdoll collision profile. The transition still depends on a valid death montage and compatible collision/physics setup.


## 12. Guard, poise, and defense tuning

`UCombatComponent` owns the defense state and the following defaults:

| Property | Default | Purpose |
| --- | ---: | --- |
| `GuardHalfAngleDegrees` | 90 | Half-angle of the planar frontal guard arc; 90 preserves the previous front half-plane |
| `ParryHalfAngleDegrees` | 70 | Half-angle of the planar parry arc |
| `bHeavyHitsBypassGuard` | true | Heavy hits break guard and continue to health |
| `bCriticalHitsBypassGuard` | false | Critical hits are blockable by default |
| `GuardBreakFallbackDuration` | 1.0 s | C++ recovery if the break montage is missing or cannot play |
| `ParryCancelBlendOutDuration` | 0.08 s | Blend-out when a parry attempt is cancelled |
| `StaggerAttackBlendOutDuration` | 0.08 s | Blend-out when poise break interrupts the character's active attack |
| `GuardReleaseBlendOutDuration` | 0.08 s | Blend-out when guard is released during its begin montage |
| `ParryWindowDuration` | 0 | Zero uses the defender parry montage length |
| `ParryCooldown` | 0.6 s | Cooldown after a whiff/cancel only |
| `PerfectParryWindowDuration` | 0.18 s | Time from parry start in which a successful parry is perfect |
| `RiposteWindowDuration` | 1.25 s | Time allowed to start the riposte |
| `RiposteDamageMultiplier` | 1.5 | Multiplier for the riposte attack |

Poise is the guard stamina pool and the stagger meter. Configure these on `UDamageHandlerComponent`:

| Property | Default | Purpose |
| --- | ---: | --- |
| `PoiseRegenerationDelay` | 2.0 s | Delay after the last poise damage before regeneration |
| `PoiseRegenerationRate` | 20 units/s | Regeneration rate |
| `PoiseRegenerationInterval` | 0.1 s | Timer interval while regeneration is active |
| `PoiseBreakStaggerDuration` | 0.85 s | C++ stagger duration when unguarded poise reaches zero |

Every blocked frontal hit spends its `FDamageInfo.PoiseDamage` and deals no health damage. When guard poise reaches zero, guard breaks and guard input is rejected until poise regenerates above zero. When unguarded poise reaches zero, the character enters `ECombatState::Staggered`. Regeneration uses a delayed timer, not a permanent tick. `OnPoiseBroken` is available for UI, audio, or Blueprint reactions.

The guard-break montage length owns recovery when it successfully starts. Otherwise `GuardBreakFallbackDuration` owns recovery. Blueprint may still call `EndGuardBreak` for an authored early exit, but gameplay no longer depends on a notify or Blueprint event to leave the state.

## 13. Ranged defense and feedback

Melee and projectiles use `UDamageReactionComponent::ResolveIncomingHit`. Projectiles pass `bAllowParry=false`: they can be blocked by the frontal guard arc and spend poise, but cannot grant or preserve a melee parry. Rear ranged hits break guard and damage health.

Per weapon, configure optional feedback assets:

- `GuardBlockNiagara` / `GuardBlockSound`
- `GuardBreakNiagara` / `GuardBreakSound`
- `ParryNiagara` / `ParrySound`

On `DamageReactionComponent`, configure optional `DefenseCameraShakeClass` and `DefenseForceFeedbackEffect`, plus the timing/scaling values:

| Property | Default |
| --- | ---: |
| `BlockHitStopDuration` | 0.035 s |
| `ParryHitStopDuration` | 0.06 s |
| `GuardBreakHitStopDuration` | 0.05 s |
| `DefenseHitStopTimeDilation` | 0.05 |
| `ParryAttackStopBlendOutDuration` | 0.04 s |
| `PerfectParrySlowMotionDuration` | 0.18 s |
| `PerfectParryTimeDilation` | 0.25 |
| `DefenseCameraShakeScale` | 0.65 |

Force-feedback intensity is authored in the `UForceFeedbackEffect` asset. Camera shake and rumble are optional until assets are assigned. No Content assets are created or modified by these code changes.

## 14. Enemy combat setup

For ordinary melee enemy Blueprints:

- Set `HeavyAttackChance` to tune the chance of requesting a heavy attack.
- Assign `HeavyAttackTelegraphMontage`. If it is not assigned or cannot play, the AI falls back to a light attack instead of issuing an untelegraphed guard-bypassing heavy.
- Tune `HeavyAttackTelegraphDuration`; zero uses the montage length.
- Configure the weapon's `AttackDamageConfigs` with `bOverrideAttackType` and `bIsHeavyAttack` to identify heavy entries. AI selects a matching attack entry/section.
- `MaxAttackersPerTarget` is clamped to 1-2 and defaults to 2. The slot is released when the attack ends, the enemy is damaged, the task aborts, or the actor ends play.
- For mage enemies, `MagicProjectileFallbackDelay=0` uses the magic attack montage length to release the projectile if its release notify is missing. A notify release clears the fallback timer, preventing duplicate projectiles.

New enemy AI defaults:

| Property | Default |
| --- | ---: |
| `HeavyAttackChance` | 0.25 |
| `HeavyAttackTelegraphDuration` | 0.55 s |
| `MaxAttackersPerTarget` | 2 (hard maximum) |
| `GuardEngageRange` | 340 cm |
| `ParryAttemptInterval` | 0.75 s |
| `ParryAttemptChance` | 0.7 |

Create a Blueprint child of `AGuardEnemyCharacter` for the defensive archetype. Assign its weapon's guard/parry montages and tune `GuardEngageRange`, `ParryAttemptInterval`, and `ParryAttemptChance`. It enters guard and attempts parries through `SetGuardMode` and `BeginParryAttempt`; its guard can be broken through the same poise system.

Enemy behavior trees pause during guard break and stagger. They resume after the C++ recovery timer or after the active reaction montage ends. Dodge/evade behavior is not part of this upgrade.

## 15. Manual guard/parry verification checklist

Run these in PIE with a player character and at least two enemy types. Expected results are included so failures are easy to isolate.

1. **Ordinary frontal block:** Hold guard and receive a light melee hit from inside the guard arc. Health remains unchanged, poise decreases, block VFX/SFX play, and guard remains active unless poise reaches zero.
2. **Rear hit:** Hold guard and receive a hit from behind. Health decreases, guard breaks, and the weapon's break montage plays.
3. **Heavy bypass:** Configure an attack entry as heavy and enable `bHeavyHitsBypassGuard`. A frontal hit breaks guard and deals damage. Confirm the heavy telegraph plays first in AI combat.
4. **Critical toggle:** Set critical chance to 1.0. With `bCriticalHitsBypassGuard=false`, a frontal critical hit is blocked and spends poise. Toggle it true and verify it breaks guard and deals damage.
5. **Guard-break recovery:** Test with a valid break montage, no montage, and a montage interrupted by another reaction. Each case exits `GuardBroken` without a Blueprint notify. Repeat through death, finisher, and weapon swap.
6. **Authored parry window:** Add `Parry Window` to an enemy attack montage. Start guard, press parry, and get hit during the notify. The attacker stops, no normal hit VFX appears, parry damage/poise damage apply, and the attacker-parried montage plays.
7. **Trace fallback:** Remove the notify state from the attack montage. Parry succeeds only while the attack's melee trace notify is active.
8. **Parry whiff/cooldown:** Press parry without an incoming parryable attack. The attempt ends at the configured duration, returns to guard, and another attempt is rejected until cooldown expires.
9. **Successful parry cooldown:** Perform a successful parry. Verify no whiff cooldown is applied. If it lands inside `PerfectParryWindowDuration`, verify the riposte window starts.
10. **Riposte:** Attack during the perfect-parry riposte window. Guard/parry state exits and the attack receives `RiposteDamageMultiplier`. Wait for the window to expire and confirm attacks from guard are blocked again.
11. **Parry interruption:** While parrying, get hit by an arrow or spell. The parry attempt ends, the ranged hit does not count as a parry, and the selected hit reaction can replace the parry montage without leaving `Parrying` stuck.
12. **Ranged guard:** Hold guard against an arrow and a magic projectile from the front. Both are blocked and spend poise; neither grants or preserves parry. Repeat from behind and verify guard break plus health damage.
13. **Arc boundaries:** Test attacks at the front, both edges of the configured guard/parry half-angles, and behind. Confirm the planar arc behaves consistently on slopes.
14. **Poise break:** Take repeated blocked hits until poise reaches zero. Guard breaks once and `OnPoiseBroken` broadcasts once. Release guard, wait the regen delay, and verify poise regenerates on the configured timer.
15. **Unguarded stagger:** Take repeated unguarded poise damage until zero. Combat enters `Staggered`, attack/guard input is rejected, then C++ recovery returns to `Idle`.
16. **Enemy AI stagger/parry:** Parry or stagger an enemy. Its BT stops issuing movement/attacks during the reaction and resumes after recovery.
17. **Attack-slot cap:** Put three enemies in range of one target. At most two may reserve attack slots at once; the third waits. Kill, interrupt, or abort one attacker and verify the waiting enemy can acquire the freed slot.
18. **Guard archetype:** Use an `AGuardEnemyCharacter` child. It enters guard at range, attempts parries during the target's parryable attack window, and can be guard-broken by heavy/rear hits or poise depletion.
19. **Feedback:** Assign block/parry/guard-break VFX and SFX, camera shake, and force-feedback assets. Verify each event plays the correct response. Confirm a blocked hit does not also play ordinary weapon hit VFX.
20. **No asset changes:** Confirm the upgrade did not modify or delete any files under `Content/`.

## 16. Defaults and verification status

New defaults are listed in sections 12-14. The only deliberate behavior-default change from the previous implementation is `bCriticalHitsBypassGuard=false`: frontal critical hits are blockable unless explicitly enabled. `bHeavyHitsBypassGuard=true` and a 90-degree guard half-angle preserve the previous heavy-bypass and front-half-plane behavior. All other new defaults establish previously missing timers, stamina, feedback, AI selection, and recovery behavior.

The code has been reviewed against the Unreal Engine 5.8 API for global time dilation and force-feedback parameters, but this environment cannot run the project's Unreal build or PIE session. Compile and runtime verification remain required.

## 17. Investor-demo acceptance pass

Before recording the demo, run the full manual checklist above with the actual player and enemy AnimBPs. In particular, confirm that every weapon has compatible guard/parry montages and that every heavy AI attack has a readable telegraph. Tune montage blend-outs and feedback assets per weapon rather than relying on shared placeholder values. The feature deliberately does not add dodge/evade.
