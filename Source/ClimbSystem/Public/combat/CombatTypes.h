// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PhysicsEngine/PhysicsBodyInstanceOwnerInterface.h"
#include "CombatTypes.generated.h"

class AWeaponBase;
class UAnimMontage;
class UCurveFloat;

/**
 * Supported weapon classifications. Extensible for project-specific weapons.
 */
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Unarmed        UMETA(DisplayName = "Unarmed / Martial Arts"),
	Sword          UMETA(DisplayName = "One-Handed / Longsword"),
	BullHeadHammer UMETA(DisplayName = "Bull Head Hammer"),
	Spear          UMETA(DisplayName = "Spear"),
	Bow            UMETA(DisplayName = "Bow (Ranged)"),
	CustomWeapon   UMETA(DisplayName = "Custom Extensible Weapon")
};

/**
 * Explicit combat state machine tracking character combat lifecycle.
 */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
	Idle           UMETA(DisplayName = "Idle"),
	Attacking      UMETA(DisplayName = "Attacking"),
	ComboWindow    UMETA(DisplayName = "Combo Window Active"),
	Recovering     UMETA(DisplayName = "Recovering / Blend Out"),
	SwappingWeapon UMETA(DisplayName = "Swapping Weapon"),
	Staggered      UMETA(DisplayName = "Staggered / Hit Stun"),
	Finishing      UMETA(DisplayName = "Executing Finisher"),
	Dead           UMETA(DisplayName = "Dead"),
	DrawingBow     UMETA(DisplayName = "Drawing Bow")
};

/** Shared animation stance selected from the currently equipped weapon. */
UENUM(BlueprintType)
enum class ECombatAnimationStyle : uint8
{
	NonCombat UMETA(DisplayName = "Noncombat"),
	Unarmed   UMETA(DisplayName = "Unarmed"),
	Sword     UMETA(DisplayName = "Sword"),
	Hammer    UMETA(DisplayName = "Hammer"),
	Spear     UMETA(DisplayName = "Spear"),
	Bow       UMETA(DisplayName = "Bow"),
	Custom    UMETA(DisplayName = "Custom")
};

/** Shared action label for AnimBP transitions. */
UENUM(BlueprintType)
enum class ECombatAnimationAction : uint8
{
	None       UMETA(DisplayName = "None"),
	Equipping  UMETA(DisplayName = "Equipping"),
	Unequipping UMETA(DisplayName = "Unequipping"),
	Attacking  UMETA(DisplayName = "Attacking"),
	Drawing    UMETA(DisplayName = "Drawing"),
	Staggered  UMETA(DisplayName = "Staggered"),
	Dead       UMETA(DisplayName = "Dead")
};

/** One compact animation-facing snapshot shared by the Combat Component and AnimInstance. */
USTRUCT(BlueprintType)
struct FCombatAnimationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	ECombatAnimationStyle Style = ECombatAnimationStyle::NonCombat;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	ECombatAnimationAction Action = ECombatAnimationAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	EWeaponType WeaponType = EWeaponType::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	ECombatState CombatState = ECombatState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	bool bWeaponTransitionActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	bool bIsDrawingBow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	float BowDrawAlpha = 0.0f;

	/** Incremented on meaningful state transitions; useful for transition events and debugging. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	int32 Revision = 0;
};

/** Selects whether a weapon uses one sectioned montage or a montage per combo step. */
UENUM(BlueprintType)
enum class EAttackMontageMode : uint8
{
	SingleComboMontage UMETA(DisplayName = "Single Montage With Sections"),
	IndividualAttackMontages UMETA(DisplayName = "One Montage Per Combo Step")
};

/** Notify point used by equip/unequip montages to move weapons between sockets. */
UENUM(BlueprintType)
enum class EWeaponSwapNotifyAction : uint8
{
	HolsterCurrent UMETA(DisplayName = "Holster Current Weapon"),
	DrawTarget UMETA(DisplayName = "Draw Target Weapon")
};

/**
 * Damage category classification for mitigation, sound, and VFX responses.
 */
UENUM(BlueprintType)
enum class EDamageCategory : uint8
{
	Physical UMETA(DisplayName = "Physical Generic"),
	Slashing UMETA(DisplayName = "Slashing (Sword)"),
	Blunt    UMETA(DisplayName = "Blunt / Crushing (Hammer)"),
	Bludgeon UMETA(DisplayName = "Hand-to-Hand Impact (Unarmed)"),
	Piercing UMETA(DisplayName = "Piercing Thrust")
};

/** Supported damage-over-time effect families. */
UENUM(BlueprintType)
enum class EDamageOverTimeType : uint8
{
	Poison,
	Burning,
	Bleeding,
	Custom
};

/** Determines how a repeated effect behaves when applied again. */
UENUM(BlueprintType)
enum class EDamageEffectStackingPolicy : uint8
{
	Ignore,
	Refresh,
	Stack
};

/** Independent, reusable damage-over-time configuration. */
USTRUCT(BlueprintType)
struct FDamageOverTimeEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time")
	FName EffectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time")
	EDamageOverTimeType Type = EDamageOverTimeType::Poison;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time", meta = (ClampMin = "0.0"))
	float DamagePerTick = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time", meta = (ClampMin = "0.05"))
	float Duration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time", meta = (ClampMin = "0.05"))
	float TickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time")
	EDamageCategory DamageCategory = EDamageCategory::Physical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time")
	EDamageEffectStackingPolicy StackingPolicy = EDamageEffectStackingPolicy::Refresh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxStacks = 5;
};

/** Damage parameters for one combo step. Missing entries use the weapon defaults. */
USTRUCT(BlueprintType)
struct FAttackDamageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage")
	bool bOverrideBaseDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage", meta = (EditCondition = "bOverrideBaseDamage", ClampMin = "0.0"))
	float BaseDamage = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	/** Probability in the inclusive range 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage", meta = (ClampMin = "1.0"))
	float CriticalDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage")
	bool bOverrideCriticalSettings = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage")
	bool bOverrideDamageOverTimeEffects = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack Damage")
	TArray<FDamageOverTimeEffect> DamageOverTimeEffects;
};

/** Coarse anatomical region resolved from a hit result. */
UENUM(BlueprintType)
enum class EDamageHitLocation : uint8
{
	Unknown,
	Head,
	Torso,
	Arms,
	Legs
};

/** Direction from which the target received the hit, in target-local space. */
UENUM(BlueprintType)
enum class EDamageHitDirection : uint8
{
	Unknown,
	Front,
	Back,
	Left,
	Right
};

/**
 * Structured payload dispatched to DamageHandlerComponent and damage delegates.
 */
USTRUCT(BlueprintType)
struct FDamageInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	float BaseDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	float PoiseDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	float KnockbackImpulse = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	EDamageCategory DamageCategory = EDamageCategory::Physical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	EWeaponType WeaponType = EWeaponType::Unarmed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage", meta = (ClampMin = "1.0"))
	float CriticalDamageMultiplier = 2.0f;

	/** Set by DamageHandlerComponent after its critical-hit roll. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Damage")
	bool bIsCriticalHit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage Over Time")
	TArray<FDamageOverTimeEffect> DamageOverTimeEffects;

	/** World-space direction of travel from attacker/projectile toward the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	FVector DamageDirection = FVector::ZeroVector;

	/** True for periodic effect ticks, which should not trigger ordinary hit reactions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Damage")
	bool bIsDamageOverTime = false;

	/** Resolved by DamageHandlerComponent from the hit bone or impact height. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Damage")
	EDamageHitLocation HitLocation = EDamageHitLocation::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Damage")
	EDamageHitDirection HitDirection = EDamageHitDirection::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	TWeakObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	TWeakObjectPtr<AWeaponBase> InstigatorWeapon = nullptr;

	/**
	 * Canonical calculated raw damage before armor or resistance mitigation.
	 * Guards against non-finite values (NaN / Inf) and clamps BaseDamage and Multiplier to >= 0.0.
	 * Preserves intentional zero damage and applies the multiplier exactly once.
	 */
	FORCEINLINE float GetCalculatedRawDamage() const
	{
		if (!FMath::IsFinite(BaseDamage) || !FMath::IsFinite(DamageMultiplier))
		{
			return 0.0f;
		}
		const float ValidBase = FMath::Max(0.0f, BaseDamage);
		const float ValidMultiplier = FMath::Max(0.0f, DamageMultiplier);
		return ValidBase * ValidMultiplier;
	}

	/**
	 * Normalizes all numerical fields in-place to ensure no non-finite or negative values propagate.
	 * Preserves legitimate zero damage while guaranteeing non-negative finite bounds.
	 */
	FORCEINLINE void Normalize()
	{
		BaseDamage = (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.0f) ? 0.0f : BaseDamage;
		DamageMultiplier = (!FMath::IsFinite(DamageMultiplier) || DamageMultiplier < 0.0f) ? 0.0f : DamageMultiplier;
		PoiseDamage = (!FMath::IsFinite(PoiseDamage) || PoiseDamage < 0.0f) ? 0.0f : PoiseDamage;
		KnockbackImpulse = (!FMath::IsFinite(KnockbackImpulse) || KnockbackImpulse < 0.0f) ? 0.0f : KnockbackImpulse;
		CriticalChance = FMath::Clamp(FMath::IsFinite(CriticalChance) ? CriticalChance : 0.0f, 0.0f, 1.0f);
		CriticalDamageMultiplier = FMath::Max(1.0f, FMath::IsFinite(CriticalDamageMultiplier) ? CriticalDamageMultiplier : 1.0f);
		for (FDamageOverTimeEffect& Effect : DamageOverTimeEffects)
		{
			Effect.DamagePerTick = FMath::Max(0.0f, FMath::IsFinite(Effect.DamagePerTick) ? Effect.DamagePerTick : 0.0f);
			Effect.Duration = FMath::Max(0.05f, FMath::IsFinite(Effect.Duration) ? Effect.Duration : 0.05f);
			Effect.TickInterval = FMath::Max(0.05f, FMath::IsFinite(Effect.TickInterval) ? Effect.TickInterval : 0.05f);
			Effect.MaxStacks = FMath::Clamp(Effect.MaxStacks, 1, 10);
		}
	}
};

/** Damage falloff is evaluated using total projectile travel distance. */
UENUM(BlueprintType)
enum class EProjectileFalloffMode : uint8
{
	None   UMETA(DisplayName = "No Falloff"),
	Linear UMETA(DisplayName = "Linear Falloff"),
	Curve  UMETA(DisplayName = "Curve Falloff")
};

/** Runtime and damage settings copied onto each spawned projectile. */
USTRUCT(BlueprintType)
struct FProjectileConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float InitialSpeed = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float MaxSpeed = 4200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement")
	float GravityScale = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement", meta = (ClampMin = "0.05"))
	float LifeSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Collision", meta = (ClampMin = "1.0"))
	float CollisionRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage", meta = (ClampMin = "0.0"))
	float PoiseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage", meta = (ClampMin = "0.0"))
	float KnockbackImpulse = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage")
	EDamageCategory DamageCategory = EDamageCategory::Piercing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage")
	EWeaponType WeaponType = EWeaponType::Bow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage", meta = (ClampMin = "1.0"))
	float CriticalDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage Over Time")
	TArray<FDamageOverTimeEffect> DamageOverTimeEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Falloff")
	EProjectileFalloffMode FalloffMode = EProjectileFalloffMode::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Falloff", meta = (ClampMin = "0.0"))
	float FalloffStartDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Falloff", meta = (ClampMin = "0.0"))
	float FalloffEndDistance = 5000.0f;

	/** Damage multiplier at and beyond FalloffEndDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Falloff", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDamageMultiplier = 0.25f;

	/** Optional curve sampled with normalized distance from falloff start (0) to end (1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Falloff", meta = (EditCondition = "FalloffMode == EProjectileFalloffMode::Curve"))
	TObjectPtr<UCurveFloat> FalloffCurve = nullptr;
};

/**
 * Data-driven configuration struct for weapon attributes and runtime behavior.
 */
USTRUCT(BlueprintType)
struct FWeaponConfig : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identification")
	EWeaponType WeaponType = EWeaponType::Sword;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identification")
	FText WeaponDisplayName = FText::FromString(TEXT("Default Weapon"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	EAttackMontageMode AttackMontageMode = EAttackMontageMode::SingleComboMontage;

	/** Used only in SingleComboMontage mode; each combo step is a montage section. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation", meta = (EditCondition = "AttackMontageMode == EAttackMontageMode::SingleComboMontage"))
	TObjectPtr<UAnimMontage> AttackComboMontage = nullptr;

	/** Used only in IndividualAttackMontages mode; array order is combo order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation", meta = (EditCondition = "AttackMontageMode == EAttackMontageMode::IndividualAttackMontages"))
	TArray<TObjectPtr<UAnimMontage>> IndividualAttackMontages;

	/** Played when switching away from this weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimMontage> UnequipMontage = nullptr;

	/** Played when switching to this weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	float BaseDamage = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	float PoiseDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	float KnockbackImpulse = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	EDamageCategory DamageCategory = EDamageCategory::Slashing;

	/** Optional per-combo-step overrides; array index matches combo section/attack index. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage")
	TArray<FAttackDamageConfig> AttackDamageConfigs;

	/** Weapon-wide defaults used when no per-attack override is configured. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage", meta = (ClampMin = "1.0"))
	float CriticalDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage Over Time")
	TArray<FDamageOverTimeEffect> DamageOverTimeEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName HandSocketName = FName("Hand_R_Socket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName HolsterSocketName = FName("Holster_Back_Socket");

	/** Sockets sampled along the weapon geometry for swept continuous collision */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Trace")
	TArray<FName> TraceSocketNames = { FName("Trace_Start"), FName("Trace_Mid"), FName("Trace_End") };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Trace")
	float TraceRadius = 14.0f;

	/** Maximum performance-capped interpolation substeps per tick for fast weapon rotations / large frame deltas */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxTraceSubsteps = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Trace")
	TEnumAsByte<ECollisionChannel> TraceCollisionChannel = ECC_Pawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Trace")
	bool bDrawDebugTraces = false;
};
