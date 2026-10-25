// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "combat/CombatTypes.h"
#include "CombatAnimInstance.generated.h"

class ACharacter;
class UCombatComponent;

/**
 * Optional shared AnimInstance base class for combat characters.
 * AnimBPs inheriting this class receive the centralized combat state and locomotion values automatically.
 */
UCLASS(Transient, Blueprintable, BlueprintType)
class CLIMBSYSTEM_API UCombatAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation")
	FCombatAnimationState CombatAnimationState;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation|Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation|Locomotion")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation|Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Animation|Locomotion")
	bool bIsAccelerating = false;

	UFUNCTION(BlueprintPure, Category = "Combat Animation")
	UCombatComponent* GetCombatComponent() const;

	/** Fired when the component's state revision changes, not for every locomotion update. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat Animation")
	void OnCombatAnimationStateChanged(const FCombatAnimationState& PreviousState, const FCombatAnimationState& NewState);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void RefreshCachedReferences();

	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UCombatComponent> CachedCombatComponent;
	int32 LastAnimationStateRevision = INDEX_NONE;
};
