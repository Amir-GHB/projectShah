// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/DamageHandlerComponent.h"
#include "combat/CombatComponent.h"
#include "combat/EnemyCharacter.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

UDamageHandlerComponent::UDamageHandlerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDamageHandlerComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(0.0f, MaxHealth);
	CurrentPoise = FMath::Max(0.0f, MaxPoise);
	bIsDead = CurrentHealth <= 0.0f;
}

float UDamageHandlerComponent::ApplyDamage_Implementation(AActor* InstigatorActor, AActor* TargetActor, const FDamageInfo& DamageInfo, const FHitResult& HitResult)
{
	if (bIsDead) return 0.0f;

	// Any real damage reaching health resolution ends an outstanding parry attempt.
	// The parry resolver clears successful attempts before applying its reward damage.
	if (AActor* OwnerActor = GetOwner())
	{
		if (UCombatComponent* Combat = OwnerActor->FindComponentByClass<UCombatComponent>())
		{
			if (Combat->IsParryAttemptActive())
			{
				Combat->EndParryAttempt(false);
			}
		}
	}

	// Shared safety net for Blueprint and future damage sources, in addition to
	// weapon-level filtering that suppresses friendly hit presentation.
	if (Cast<AEnemyCharacter>(InstigatorActor) && Cast<AEnemyCharacter>(TargetActor))
	{
		return 0.0f;
	}

	FDamageInfo ResolvedInfo = DamageInfo;
	ResolvedInfo.Normalize();
	ResolvedInfo.HitLocation = ResolveHitLocation(HitResult, TargetActor);
	ResolvedInfo.HitDirection = ResolveHitDirection(ResolvedInfo, TargetActor);

	float RawDamage = ResolvedInfo.GetCalculatedRawDamage()
		* GetHitLocationMultiplier(ResolvedInfo.HitLocation);
	if (!ResolvedInfo.bCriticalRollResolved)
	{
		ResolvedInfo.bIsCriticalHit = ResolvedInfo.CriticalChance > 0.0f
			&& FMath::FRand() < ResolvedInfo.CriticalChance;
		ResolvedInfo.bCriticalRollResolved = true;
	}

	if (ResolvedInfo.bIsCriticalHit)
	{
		RawDamage *= ResolvedInfo.CriticalDamageMultiplier;
	}

	const float SafeArmor = FMath::Max(0.0f, FMath::IsFinite(Armor) ? Armor : 0.0f);
	const float MitigationFactor = 100.0f / (100.0f + SafeArmor);
	const float FinalDamage = FMath::Max(0.0f, RawDamage * MitigationFactor);

	CurrentPoise = FMath::Clamp(CurrentPoise - ResolvedInfo.PoiseDamage, 0.0f, FMath::Max(0.0f, MaxPoise));
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, FMath::Max(0.0f, MaxHealth));
	LastDamageInfo = ResolvedInfo;

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnDamageTaken.Broadcast(FinalDamage, InstigatorActor, ResolvedInfo, HitResult);

	if (CurrentHealth <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		ActiveDamageOverTimeEffects.Empty();
		SetComponentTickEnabled(false);
		OnDeath.Broadcast(InstigatorActor);
	}
	else
	{
		AddDamageOverTimeEffects(ResolvedInfo, InstigatorActor);
	}

	return FinalDamage;
}

EDamageHitLocation UDamageHandlerComponent::ResolveHitLocation(
	const FHitResult& HitResult, AActor* TargetActor) const
{
	if (!HitResult.BoneName.IsNone())
	{
		if (const EDamageHitLocation* MappedLocation = BoneHitLocationMap.Find(HitResult.BoneName))
		{
			return *MappedLocation;
		}
	}

	if (!IsValid(TargetActor))
	{
		return EDamageHitLocation::Unknown;
	}

	UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	const FBox Bounds = IsValid(HitComponent)
		? HitComponent->Bounds.GetBox()
		: TargetActor->GetComponentsBoundingBox(true);
	const float Height = Bounds.GetSize().Z;
	if (Height <= KINDA_SMALL_NUMBER)
	{
		return EDamageHitLocation::Unknown;
	}

	const float HitHeight = HitResult.ImpactPoint.IsNearlyZero()
		? TargetActor->GetActorLocation().Z : HitResult.ImpactPoint.Z;
	const float HeightAlpha = FMath::Clamp(
		(HitHeight - Bounds.Min.Z) / Height, 0.0f, 1.0f);

	if (HeightAlpha >= 0.82f) return EDamageHitLocation::Head;
	if (HeightAlpha <= 0.35f) return EDamageHitLocation::Legs;
	return EDamageHitLocation::Torso;
}

EDamageHitDirection UDamageHandlerComponent::ResolveHitDirection(
	const FDamageInfo& DamageInfo, AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return EDamageHitDirection::Unknown;
	}

	FVector DirectionToTarget = DamageInfo.DamageDirection.GetSafeNormal();
	if (DirectionToTarget.IsNearlyZero())
	{
		if (AActor* Source = DamageInfo.DamageCauser.Get())
		{
			DirectionToTarget = (TargetActor->GetActorLocation()
				- Source->GetActorLocation()).GetSafeNormal();
		}
	}

	if (DirectionToTarget.IsNearlyZero())
	{
		return EDamageHitDirection::Unknown;
	}

	const FVector ToAttackerLocal = TargetActor->GetActorTransform()
		.InverseTransformVectorNoScale(-DirectionToTarget).GetSafeNormal();

	if (FMath::Abs(ToAttackerLocal.X) >= FMath::Abs(ToAttackerLocal.Y))
	{
		return ToAttackerLocal.X >= 0.0f
			? EDamageHitDirection::Front : EDamageHitDirection::Back;
	}

	return ToAttackerLocal.Y >= 0.0f
		? EDamageHitDirection::Right : EDamageHitDirection::Left;
}

float UDamageHandlerComponent::GetHitLocationMultiplier(
	EDamageHitLocation HitLocation) const
{
	switch (HitLocation)
	{
	case EDamageHitLocation::Head: return FMath::Max(0.0f, HeadDamageMultiplier);
	case EDamageHitLocation::Torso: return FMath::Max(0.0f, TorsoDamageMultiplier);
	case EDamageHitLocation::Arms: return FMath::Max(0.0f, ArmDamageMultiplier);
	case EDamageHitLocation::Legs: return FMath::Max(0.0f, LegDamageMultiplier);
	default: return 1.0f;
	}
}

FName UDamageHandlerComponent::GetDamageEffectKey(const FDamageOverTimeEffect& Effect)
{
	if (!Effect.EffectId.IsNone())
	{
		return Effect.EffectId;
	}

	return FName(*FString::Printf(TEXT("DamageEffect_%d"), static_cast<int32>(Effect.Type)));
}

void UDamageHandlerComponent::AddDamageOverTimeEffects(const FDamageInfo& DamageInfo, AActor* InstigatorActor)
{
	if (bIsDead || DamageInfo.DamageOverTimeEffects.IsEmpty()) return;

	for (const FDamageOverTimeEffect& Definition : DamageInfo.DamageOverTimeEffects)
	{
		if (Definition.DamagePerTick <= 0.0f || Definition.Duration <= 0.0f || Definition.TickInterval <= 0.0f)
		{
			continue;
		}

		const FName EffectKey = GetDamageEffectKey(Definition);
		FActiveDamageOverTimeEffect* Existing = ActiveDamageOverTimeEffects.FindByPredicate(
			[&EffectKey](const FActiveDamageOverTimeEffect& Active)
			{
				return UDamageHandlerComponent::GetDamageEffectKey(Active.Definition) == EffectKey;
			});

		if (Existing)
		{
			switch (Definition.StackingPolicy)
			{
			case EDamageEffectStackingPolicy::Ignore:
				continue;
			case EDamageEffectStackingPolicy::Refresh:
				Existing->Definition = Definition;
				Existing->InstigatorActor = InstigatorActor;
				Existing->DamageCauser = DamageInfo.DamageCauser;
				Existing->ElapsedTime = 0.0f;
				Existing->NextTickTime = Definition.TickInterval;
				Existing->StackCount = 1;
				continue;
			case EDamageEffectStackingPolicy::Stack:
				Existing->Definition = Definition;
				Existing->InstigatorActor = InstigatorActor;
				Existing->DamageCauser = DamageInfo.DamageCauser;
				Existing->ElapsedTime = 0.0f;
				Existing->NextTickTime = Definition.TickInterval;
				Existing->StackCount = FMath::Min(Existing->StackCount + 1, Definition.MaxStacks);
				continue;
			default:
				continue;
			}
		}

		FActiveDamageOverTimeEffect& NewEffect = ActiveDamageOverTimeEffects.AddDefaulted_GetRef();
		NewEffect.Definition = Definition;
		NewEffect.InstigatorActor = InstigatorActor;
		NewEffect.DamageCauser = DamageInfo.DamageCauser;
		NewEffect.ElapsedTime = 0.0f;
		NewEffect.NextTickTime = Definition.TickInterval;
		NewEffect.StackCount = 1;
	}

	SetComponentTickEnabled(!ActiveDamageOverTimeEffects.IsEmpty());
}

void UDamageHandlerComponent::ApplyDamageOverTimeTick(const FActiveDamageOverTimeEffect& ActiveEffect)
{
	if (bIsDead || !GetOwner()) return;

	FDamageInfo TickInfo;
	TickInfo.bIsDamageOverTime = true;
	TickInfo.BaseDamage = ActiveEffect.Definition.DamagePerTick * ActiveEffect.StackCount;
	TickInfo.DamageCategory = ActiveEffect.Definition.DamageCategory;
	TickInfo.DamageCauser = ActiveEffect.DamageCauser;
	TickInfo.Normalize();
	LastDamageInfo = TickInfo;

	const float SafeArmor = FMath::Max(0.0f, FMath::IsFinite(Armor) ? Armor : 0.0f);
	const float FinalDamage = TickInfo.GetCalculatedRawDamage() * (100.0f / (100.0f + SafeArmor));
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, FMath::Max(0.0f, MaxHealth));

	FHitResult EmptyHit;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnDamageTaken.Broadcast(FinalDamage, ActiveEffect.InstigatorActor.Get(), TickInfo, EmptyHit);

	if (CurrentHealth <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		ActiveDamageOverTimeEffects.Empty();
		SetComponentTickEnabled(false);
		OnDeath.Broadcast(ActiveEffect.InstigatorActor.Get());
	}
}

void UDamageHandlerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (int32 Index = ActiveDamageOverTimeEffects.Num() - 1; Index >= 0; --Index)
	{
		if (bIsDead || !ActiveDamageOverTimeEffects.IsValidIndex(Index)) break;

		ActiveDamageOverTimeEffects[Index].ElapsedTime += DeltaTime;

		// Copy the effect before broadcasting damage delegates. A listener may apply another
		// effect and reallocate the array, so never retain an array-element reference here.
		while (ActiveDamageOverTimeEffects.IsValidIndex(Index) && !bIsDead)
		{
			const FActiveDamageOverTimeEffect EffectSnapshot = ActiveDamageOverTimeEffects[Index];
			if (EffectSnapshot.NextTickTime > EffectSnapshot.Definition.Duration
				|| EffectSnapshot.ElapsedTime + KINDA_SMALL_NUMBER < EffectSnapshot.NextTickTime)
			{
				break;
			}

			ActiveDamageOverTimeEffects[Index].NextTickTime += EffectSnapshot.Definition.TickInterval;
			ApplyDamageOverTimeTick(EffectSnapshot);
		}

		if (!bIsDead && ActiveDamageOverTimeEffects.IsValidIndex(Index)
			&& ActiveDamageOverTimeEffects[Index].ElapsedTime >= ActiveDamageOverTimeEffects[Index].Definition.Duration)
		{
			ActiveDamageOverTimeEffects.RemoveAtSwap(Index);
		}
	}

	if (ActiveDamageOverTimeEffects.IsEmpty() || bIsDead)
	{
		ActiveDamageOverTimeEffects.Empty();
		SetComponentTickEnabled(false);
	}
}

void UDamageHandlerComponent::ResetStats()
{
	ActiveDamageOverTimeEffects.Empty();
	SetComponentTickEnabled(false);
	bIsDead = false;
	CurrentHealth = FMath::Max(0.0f, MaxHealth);
	CurrentPoise = FMath::Max(0.0f, MaxPoise);
	LastDamageInfo = FDamageInfo();
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UDamageHandlerComponent::Heal(float Amount)
{
	if (bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.0f) return;

	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, FMath::Max(0.0f, MaxHealth));
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
