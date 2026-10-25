// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "DamageHandlerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDamageTakenSignature, float, ActualDamage, AActor*, InstigatorActor, const FDamageInfo&, DamageInfo, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedSignature, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, KillerActor);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UDamageHandlerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageHandlerComponent();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Armor = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxPoise = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float CurrentPoise = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	bool bIsDead = false;

	// BlueprintNativeEvent Damage Interface
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Damage")
	float ApplyDamage(AActor* InstigatorActor, AActor* TargetActor, const FDamageInfo& DamageInfo, const FHitResult& HitResult);
	virtual float ApplyDamage_Implementation(AActor* InstigatorActor, AActor* TargetActor, const FDamageInfo& DamageInfo, const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	void Heal(float Amount);

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetHealthPercent() const { return (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnDamageTakenSignature OnDamageTaken;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnDeathSignature OnDeath;
};
