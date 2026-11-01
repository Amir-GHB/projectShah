// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/DamageHandlerComponent.h"
#include "GameFramework/Actor.h"

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

	FDamageInfo ResolvedInfo = DamageInfo;
	ResolvedInfo.Normalize();

	float RawDamage = ResolvedInfo.GetCalculatedRawDamage();
	ResolvedInfo.bIsCriticalHit = ResolvedInfo.CriticalChance > 0.0f
		&& FMath::FRand() < ResolvedInfo.CriticalChance;

	if (ResolvedInfo.bIsCriticalHit)
	{
		RawDamage *= ResolvedInfo.CriticalDamageMultiplier;
	}

	const float SafeArmor = FMath::Max(0.0f, FMath::IsFinite(Armor) ? Armor : 0.0f);
	const float MitigationFactor = 100.0f / (100.0f + SafeArmor);
	const float FinalDamage = FMath::Max(0.0f, RawDamage * MitigationFactor);

	CurrentPoise = FMath::Clamp(CurrentPoise - ResolvedInfo.PoiseDamage, 0.0f, FMath::Max(0.0f, MaxPoise));
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, FMath::Max(0.0f, MaxHealth));

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
				return GetDamageEffectKey(Active.Definition) == EffectKey;
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
	TickInfo.BaseDamage = ActiveEffect.Definition.DamagePerTick * ActiveEffect.StackCount;
	TickInfo.DamageCategory = ActiveEffect.Definition.DamageCategory;
	TickInfo.DamageCauser = ActiveEffect.DamageCauser;
	TickInfo.Normalize();

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
		if (bIsDead) break;

		FActiveDamageOverTimeEffect& Effect = ActiveDamageOverTimeEffects[Index];
		Effect.ElapsedTime += DeltaTime;

		while (Effect.NextTickTime <= Effect.Definition.Duration
			&& Effect.ElapsedTime + KINDA_SMALL_NUMBER >= Effect.NextTickTime
			&& !bIsDead)
		{
			ApplyDamageOverTimeTick(Effect);
			Effect.NextTickTime += Effect.Definition.TickInterval;
		}

		if (Effect.ElapsedTime >= Effect.Definition.Duration)
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

void UDamageHandlerComponent::Heal(float Amount)
{
	if (bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.0f) return;

	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, FMath::Max(0.0f, MaxHealth));
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
