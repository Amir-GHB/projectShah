// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTargetingComponent.generated.h"

class ACharacter;

/**
 * Attack-scoped target selection and soft focus.
 * A remembered manual target is only a preference: this component never rotates or moves
 * the owner toward a target unless BeginAttackFocus has been called by an active attack.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UCombatTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatTargetingComponent();

	/** Begin soft focus for an attack. Safe to call repeatedly across combo steps. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
	void BeginAttackFocus();

	/** End all attack-scoped rotation and movement correction immediately. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
	void EndAttackFocus();

	/** Toggle a preferred target. This does not rotate the character outside an attack. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
	void ToggleManualLock();

	UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
	void SetManualTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
	void ClearManualTarget();

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	AActor* GetManualTarget() const { return ManualTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	bool IsAttackFocusActive() const { return bAttackFocusActive; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting", meta = (ClampMin = "100.0"))
	float MaxFocusRange = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting", meta = (ClampMin = "0.02"))
	float TargetRefreshInterval = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting", meta = (ClampMin = "0.1"))
	float RotationInterpSpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting|Movement", meta = (ClampMin = "0.0"))
	float CorrectionStartDistance = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting|Movement", meta = (ClampMin = "0.0"))
	float CorrectionMaxDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CorrectionStrength = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Targeting|Movement", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CorrectionDuration = 0.12f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<ACharacter> OwningCharacter;
	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<AActor> ManualTarget;

	bool bAttackFocusActive = false;
	bool bCorrectionApplied = false;
	bool bPreviousOrientRotationToMovement = true;
	float TargetRefreshElapsed = 0.0f;
	float CorrectionTimeRemaining = 0.0f;

	AActor* FindBestTarget() const;
	bool IsValidTarget(AActor* Candidate) const;
	bool IsWithinFocusRange(const AActor* Candidate) const;
	bool HasLineOfSight(const AActor* Candidate) const;
	void SetCurrentTarget(AActor* NewTarget);
};
