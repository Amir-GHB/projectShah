// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatPositioningComponent.generated.h"

class UMotionWarpingComponent;

/**
 * Prepares an attack destination for Unreal's Motion Warping system.
 * The montage's Motion Warping notify window controls when root motion is warped.
 * Target acquisition remains the responsibility of TargetingComponent / AIController.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UCombatPositioningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatPositioningComponent();

	/**
	 * Calculates and registers the destination transform used by the attack montage.
	 * The montage must contain a Motion Warping notify window using the same target name.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Positioning")
	void BeginAttackPositioning(
		AActor* Target,
		FName WarpTargetName,
		float PreferredDistance,
		float MaxEngagementRange,
		bool bEnablePositioning = true);

	/** Removes the registered warp target when the attack ends or is interrupted. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Positioning")
	void EndAttackPositioning();

	UFUNCTION(BlueprintPure, Category = "Combat|Positioning")
	bool IsAttackPositioningActive() const { return bAttackPositioningActive; }

	UFUNCTION(BlueprintPure, Category = "Combat|Positioning")
	FTransform GetPreparedWarpTransform() const { return PreparedWarpTransform; }

private:
	bool bAttackPositioningActive = false;
	FName ActiveWarpTargetName = NAME_None;
	FTransform PreparedWarpTransform = FTransform::Identity;

	TWeakObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
};
