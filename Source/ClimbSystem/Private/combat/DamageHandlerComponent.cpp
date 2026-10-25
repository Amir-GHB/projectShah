// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/DamageHandlerComponent.h"
#include "GameFramework/Actor.h"

UDamageHandlerComponent::UDamageHandlerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDamageHandlerComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	CurrentPoise = MaxPoise;
	bIsDead = false;
}

float UDamageHandlerComponent::ApplyDamage_Implementation(AActor* InstigatorActor, AActor* TargetActor, const FDamageInfo& DamageInfo, const FHitResult& HitResult)
{
	if (bIsDead) return 0.0f;

	// Normalize incoming damage struct to guarantee non-negative finite bounds
	FDamageInfo NormalizedInfo = DamageInfo;
	NormalizedInfo.Normalize();

	// 1. Calculate raw damage using canonical formula
	const float RawDamage = NormalizedInfo.GetCalculatedRawDamage();

	// 2. Armor mitigation formula: FinalDamage = RawDamage * (100 / (100 + Armor))
	const float MitigationFactor = 100.0f / (100.0f + FMath::Max(0.0f, Armor));
	const float FinalDamage = FMath::Max(0.0f, RawDamage * MitigationFactor);

	// 3. Poise reduction (applies even on 0-damage hits such as utility or stagger attacks)
	CurrentPoise = FMath::Clamp(CurrentPoise - NormalizedInfo.PoiseDamage, 0.0f, MaxPoise);

	// 4. Health deduction (preserves legitimate 0-damage hits)
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, MaxHealth);

	// 5. Broadcast delegates
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnDamageTaken.Broadcast(FinalDamage, InstigatorActor, NormalizedInfo, HitResult);

	// 6. Death detection
	if (CurrentHealth <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		OnDeath.Broadcast(InstigatorActor);
	}

	return FinalDamage;
}

void UDamageHandlerComponent::Heal(float Amount)
{
	if (bIsDead || Amount <= 0.0f) return;

	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
