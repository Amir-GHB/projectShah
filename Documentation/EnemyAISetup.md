# Enemy AI Foundation (Behavior Tree + Blackboard)

This branch uses Unreal's native Behavior Tree and Blackboard runtime. Perception remains on the native AI controller; the controller writes observed target/investigation state to the Blackboard. Behavior Tree nodes own movement and action decisions. Existing combat components remain authoritative for weapon state, attacks, montage callbacks, damage, and combo progression.

## Native classes

- `AEnemyCharacter`: shared enemy base, combat/damage components, Behavior Tree asset reference, perception tuning, and patrol settings.
- `AEnemyAIController`: sight/hearing perception adapter, target validation, encounter registration, Blackboard updates, and combo-window forwarding. It no longer runs a periodic decision loop.
- `UBTService_EnemyState`: mirrors target, investigation, and death state into Blackboard keys.
- `UBTTask_EnemyEngageTarget`: target pursuit, archetype-aware spacing, facing, attack cadence, and task-abort cleanup.
- `UBTTask_EnemyPatrol`: ordered patrol points or random reachable NavMesh destinations, with waits and retry handling.
- `AMagicEnemyCharacter`: existing montage-notify-driven magic projectile behavior and preferred-distance relocation.
- `ABowEnemyCharacter`: AI bow draw/release timing through the existing `UCombatComponent` and equipped `AWeapon_Bow`.

## Required editor assets

The C++ runtime nodes are included in the module, but Unreal Behavior Tree and Blackboard assets are editor-authored assets and are not safely represented by source-only C++ files. Create the assets below once in the Content Browser, then assign the Behavior Tree on each enemy Blueprint.

### 1. Create the Blackboard

Create a Blackboard asset named `BB_Enemy` with these keys (spelling and type must match):

| Key | Type | Purpose |
|---|---|---|
| `TargetActor` | Object (Base Class: Actor) | Current visible player target |
| `HasTarget` | Bool | Target branch condition |
| `InvestigationLocation` | Vector | Last heard/seen location |
| `HasInvestigation` | Bool | Investigation branch condition |
| `IsDead` | Bool | Death state for optional decorators/debugging |

Assign `BB_Enemy` to the Blackboard field of the Behavior Tree.

### 2. Create the Behavior Tree

Create `BT_Enemy`, assign `BB_Enemy`, and construct this priority-ordered tree:

```text
Root
└── Selector
    ├── Sequence [Decorator: HasTarget is true, Observer Aborts: Both]
    │   └── Engage Target (UBTTask_EnemyEngageTarget)
    ├── Sequence [Decorator: HasInvestigation is true, Observer Aborts: Both]
    │   ├── Move To (Blackboard: InvestigationLocation, acceptable radius: 100)
    │   └── Wait (0.5 to 1.0 seconds)
    └── Patrol (UBTTask_EnemyPatrol)
```

Add `UBTService_EnemyState` to the root Selector (or a parent composite) and retain its default Blackboard key names. Its 0.2-second service interval updates the state used by branch decorators. Set both target/investigation decorators to observe their Blackboard key and abort lower-priority branches when the condition changes. The patrol task returns Failed when patrol is disabled, leaving the tree idle until a higher-priority branch becomes relevant.

### 3. Assign assets

For each child Blueprint of `AEnemyCharacter`:

1. Set `BehaviorTreeAsset` to `BT_Enemy`.
2. Configure the mesh, Animation Blueprint, damage/reaction settings, and `CombatComponent.WeaponClasses`.
3. Set `CombatComponent.DefaultWeaponType` to the weapon the archetype should use.
4. Tune sight, hearing, attack range, retry interval, and investigation duration.
5. Add a NavMeshBoundsVolume over the playable area and build navigation.

The enemy auto-possesses `AEnemyAIController` when placed or spawned. If no Behavior Tree is assigned, the controller logs an explicit error and does not silently fall back to the removed timer-driven state machine.

## Archetypes

### Melee

Use a child of `AEnemyCharacter`, configure its melee weapon class and default weapon, and use `BT_Enemy`. The engage task closes to `AttackRange`, faces the target, and calls `PerformAICombatAction`. Combo chaining still uses the authored `UCombatComponent` combo-window event and transition notifies.

### Mage

Use a child of `AMagicEnemyCharacter`. Configure `MagicProjectileClass`, `MagicAttackMontage`, projectile config, spawn socket/offset, cooldown, and preferred-distance multiplier. If a montage is assigned, retain the native **Release Magic Projectile** notify at the exact release frame. The engage task maintains the mage's preferred range and honors its post-shot relocation flag. Projectile release remains target-directed and montage-notify driven.

### Archer

Use a child of `ABowEnemyCharacter`. Configure its `CombatComponent` with an `AWeapon_Bow` subclass and set the default weapon to Bow. Tune `BowDrawDuration`, `BowAttackCooldown`, and `PreferredCombatDistanceMultiplier`. The archer uses `HandleAttackInput` to begin drawing and `HandleAttackReleased` to fire. Projectile configuration, draw scaling, and damage continue through the existing bow implementation. The controller's focus is used by the bow's existing control-rotation aiming path. If sight is lost or the engage task is aborted, the active draw is cancelled rather than released.

## Patrol configuration

- `bEnablePatrol` is the master switch. Disable it to stop all patrol behavior, even if patrol points are assigned.
- If `PatrolPoints` contains actors, the task visits them in array order and loops.
- If there are no points and `bUseRandomPatrolWhenNoPoints` is true, the task chooses random reachable NavMesh points within `RandomPatrolRadius`.
- If there are no points and random patrol is disabled, the patrol task returns Failed.
- `PatrolAcceptanceRadius` and `PatrolWaitTime` control arrival and pauses.

Patrol is a lower-priority branch. Acquiring a target or receiving an investigation stimulus aborts patrol movement; when the higher-priority branch ends, the selector can enter patrol again.

## Perception and sound

Sight detects player-controlled Pawns within the configured radius/cone. Losing sight stores the last seen location and clears the target; hearing stores the stimulus location. The investigation branch remains active for `InvestigateDuration`. Gameplay systems must report noise with `UAISense_Hearing::ReportNoiseEvent` (or an equivalent Pawn noise event); merely playing an audio asset does not emit a hearing stimulus.

The controller registers a newly acquired real enemy with the player's encounter tracker, preserving last-enemy finisher behavior. It also forwards the existing authored combo-window event to the combat component only while the target remains valid and within melee attack range.

## Extension boundary

Keep external decision systems outside combat implementation. Perception providers should feed the controller's target/investigation boundary; decision services can populate the same Blackboard contract or replace the service/task policy. Navigation remains behind AIController movement requests, and all attacks remain behind `PerformAICombatAction` / `UCombatComponent`. A future external integration should not own weapon, montage, damage, or character state.

## Validation

- Targets remain player-controlled Pawns only; faction/team filtering and squad coordination are not included.
- One target is tracked at a time. No cover system, squad tactics, or alert propagation is included.
- Bow AI requires an equipped bow, valid projectile class, compatible draw/release montages, and a valid arrow spawn socket (or the existing fallback).
- Mage and bow spacing are derived from `AttackRange` multiplied by their archetype-specific distance setting.
- No Behavior Tree/Blackboard `.uasset` files are committed by this source change. Create the two assets in the editor as described above and assign `BehaviorTreeAsset`.
- Source changes have not been compiled or exercised in Unreal Editor in this environment. Build in UE 5.8, then validate melee combo windows, mage release notify/relocation, bow draw/release/cancel, perception loss/investigation, patrol interruption/resume, and death interruption.
