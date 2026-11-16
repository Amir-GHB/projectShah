// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatPositioningComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/**
 * Performs short-lived, attack-scoped movement and facing corrections.
 * Target acquisition remains the responsibility of TargetingComponent / AIController.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UCombatPositioningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatPositioningComponent();

	/** Starts attack-time spacing correction for the supplied target. Replaces any active operation. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Positioning")
	void BeginAttackPositioning(
		AActor* Target,
		float PreferredDistance,
		float DistanceTolerance,
		float MaxEngagementRange,
		bool bEnablePositioning = true);

	/** Stops correction and restores the owner's previous movement-facing setting. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Positioning")
	void EndAttackPositioning();

	UFUNCTION(BlueprintPure, Category = "Combat|Positioning")
	bool IsAttackPositioningActive() const { return bAttackPositioningActive; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Positioning", meta = (ClampMin = "0.0"))
	float MaxCorrectionInput = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Positioning", meta = (ClampMin = "0.1"))
	float RotationInterpSpeed = 12.0f;

	/** Caps the additional spacing correction so it does not turn into a full-speed lunge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Positioning", meta = (ClampMin = "1.0"))
	float MaxCorrectionSpeed = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Positioning")
	bool bRotateToTargetDuringAttack = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<ACharacter> OwningCharacter;
	TWeakObjectPtr<AActor> CombatTarget;

	bool bAttackPositioningActive = false;
	bool bPreviousOrientRotationToMovement = true;
	float PreferredDistance = 160.0f;
	float DistanceTolerance = 20.0f;
	float MaxEngagementRange = 450.0f;

	void RestoreMovementFacing();
};
