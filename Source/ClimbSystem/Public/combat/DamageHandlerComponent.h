// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "CombatTypes.h"
#include "DamageHandlerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDamageTakenSignature, float, ActualDamage, AActor*, InstigatorActor, const FDamageInfo&, DamageInfo, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedSignature, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, KillerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPoiseBrokenSignature, AActor*, InstigatorActor);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UDamageHandlerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageHandlerComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	struct FActiveDamageOverTimeEffect
	{
		FDamageOverTimeEffect Definition;
		TWeakObjectPtr<AActor> InstigatorActor;
		TWeakObjectPtr<AActor> DamageCauser;
		float ElapsedTime = 0.0f;
		float NextTickTime = 0.0f;
		int32 StackCount = 1;
	};

	TArray<FActiveDamageOverTimeEffect> ActiveDamageOverTimeEffects;
	FTimerHandle PoiseRegenerationTimer;
	bool bPoiseBroken = false;
	void RegeneratePoise();

	void AddDamageOverTimeEffects(const FDamageInfo& DamageInfo, AActor* InstigatorActor);
	void ApplyDamageOverTimeTick(const FActiveDamageOverTimeEffect& ActiveEffect);
	static FName GetDamageEffectKey(const FDamageOverTimeEffect& Effect);
	EDamageHitLocation ResolveHitLocation(const FHitResult& HitResult, AActor* TargetActor) const;
	EDamageHitDirection ResolveHitDirection(const FDamageInfo& DamageInfo, AActor* TargetActor) const;
	float GetHitLocationMultiplier(EDamageHitLocation HitLocation) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Armor = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxPoise = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit Location")
	TMap<FName, EDamageHitLocation> BoneHitLocationMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit Location", meta = (ClampMin = "0.0"))
	float HeadDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit Location", meta = (ClampMin = "0.0"))
	float TorsoDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit Location", meta = (ClampMin = "0.0"))
	float ArmDamageMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit Location", meta = (ClampMin = "0.0"))
	float LegDamageMultiplier = 0.85f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float CurrentPoise = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float PoiseRegenerationDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float PoiseRegenerationRate = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Poise", meta = (ClampMin = "0.05"))
	float PoiseRegenerationInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Poise", meta = (ClampMin = "0.05"))
	float PoiseBreakStaggerDuration = 0.85f;

	UFUNCTION(BlueprintCallable, Category = "Combat|Poise")
	void ApplyPoiseDamage(float Amount, AActor* InstigatorActor = nullptr);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	bool bIsDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Damage")
	FDamageInfo LastDamageInfo;

	// BlueprintNativeEvent Damage Interface
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Damage")
	float ApplyDamage(AActor* InstigatorActor, AActor* TargetActor, const FDamageInfo& DamageInfo, const FHitResult& HitResult);
	virtual float ApplyDamage_Implementation(AActor* InstigatorActor, AActor* TargetActor, const FDamageInfo& DamageInfo, const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	void ResetStats();

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetHealthPercent() const { return (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnDamageTakenSignature OnDamageTaken;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnDeathSignature OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnPoiseBrokenSignature OnPoiseBroken;
};
