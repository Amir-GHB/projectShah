// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "riding/RidingTypes.h"
#include "RidingComponent.generated.h"

class AHorseCharacter;
class ACombatCharacterBase;
class APlayerController;
class UAnimMontage;

UCLASS(ClassGroup=(Riding), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API URidingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URidingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Riding|State")
	ERidingState RidingState = ERidingState::Unmounted;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Riding|State")
	EHorseMountDirection MountDirection = EHorseMountDirection::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Riding|State")
	TObjectPtr<AHorseCharacter> CurrentHorse = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Riding|Hiding")
	bool bIsHorseHiding = false;

	/** Animation phase for the rider AnimBP: enter montage, held loop, exit montage. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Riding|Hiding")
	EHorseHidingState HorseHidingState = EHorseHidingState::NotHiding;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Hiding")
	EHorseHideSide HorseHideSide = EHorseHideSide::Left;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Hiding")
	TObjectPtr<UAnimMontage> HideLeftMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Hiding")
	TObjectPtr<UAnimMontage> HideRightMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Hiding")
	TObjectPtr<UAnimMontage> UnhideLeftMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Hiding")
	TObjectPtr<UAnimMontage> UnhideRightMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Mounting", meta = (ClampMin = "50.0"))
	float MountSearchRadius = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Mounting", meta = (ClampMin = "0.05"))
	float MountTransitionDuration = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Mounting")
	TObjectPtr<UAnimMontage> MountLeftMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Mounting")
	TObjectPtr<UAnimMontage> MountRightMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Mounting")
	TObjectPtr<UAnimMontage> MountRearMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Dismounting")
	TObjectPtr<UAnimMontage> DismountLeftMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Dismounting")
	TObjectPtr<UAnimMontage> DismountRightMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Dismounting")
	TObjectPtr<UAnimMontage> DismountRearMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Camera", meta = (ClampMin = "0.0"))
	float CameraBlendDuration = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Riding|Dismounting")
	EHorseMountDirection PreferredDismountDirection = EHorseMountDirection::Right;

	UFUNCTION(BlueprintCallable, Category = "Riding")
	bool TryMountNearbyHorse();

	UFUNCTION(BlueprintCallable, Category = "Riding")
	bool TryMountHorse(AHorseCharacter* Horse, EHorseMountDirection Direction);

	UFUNCTION(BlueprintCallable, Category = "Riding")
	void RequestDismount();

	/** Input Started: begin entering the hidden pose. */
	UFUNCTION(BlueprintCallable, Category = "Riding|Hiding")
	bool StartHorseHiding();

	/** Input Completed/Canceled: leave the hidden pose after the enter montage finishes. */
	UFUNCTION(BlueprintCallable, Category = "Riding|Hiding")
	void StopHorseHiding();

	/** Compatibility helper for existing Blueprint callers. */
	UFUNCTION(BlueprintCallable, Category = "Riding|Hiding")
	bool ToggleHorseHiding();

	/** Compatibility helper. False performs an immediate reset for forced transitions. */
	UFUNCTION(BlueprintCallable, Category = "Riding|Hiding")
	void SetHorseHiding(bool bHide);

	UFUNCTION(BlueprintPure, Category = "Riding|Hiding")
	bool IsHiddenFromEnemies() const { return bIsHorseHiding && RidingState == ERidingState::Mounted; }

	/** Detaches a mounted rider and returns control to the dying character for its death animation. */
	void HandleRiderDeath();

	UFUNCTION(BlueprintPure, Category = "Riding")
	bool IsMounted() const { return RidingState == ERidingState::Mounted; }

	UFUNCTION(BlueprintPure, Category = "Riding")
	bool IsInRidingTransition() const { return RidingState == ERidingState::Mounting || RidingState == ERidingState::Dismounting; }

	UFUNCTION(BlueprintPure, Category = "Riding")
	AHorseCharacter* GetCurrentHorse() const { return CurrentHorse; }

	void SetRidingTransition(ERidingState NewState, EHorseMountDirection Direction);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<ACombatCharacterBase> Rider;
	TWeakObjectPtr<APlayerController> CachedPlayerController;
	TWeakObjectPtr<AHorseCharacter> CameraTransitionHorse;
	FTransform TransitionStart = FTransform::Identity;
	FTransform TransitionTarget = FTransform::Identity;
	float TransitionElapsed = 0.0f;
	bool bTransitionInProgress = false;
	ECollisionEnabled::Type PreviousCapsuleCollisionMode = ECollisionEnabled::QueryAndPhysics;
	bool bWasCameraCollisionTestEnabled = true;
	FVector DismountTargetLocation = FVector::ZeroVector;
	FRotator DismountTargetRotation = FRotator::ZeroRotator;
	FTimerHandle CameraInputRestoreTimer;

	void BeginTransformTransition(const FTransform& Target, ERidingState NewState, EHorseMountDirection Direction);
	void FinishMount();
	void FinishDismount();
	void PlayTransitionMontage(bool bMounting, EHorseMountDirection Direction);
	void SchedulePlayerInputRestore();
	void RestorePlayerInput();
	void UpdateRiderAnimationState();
	void BeginHorseHideExit();
	void FinishHorseHideExit();
	void HandleHideStartMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleHideEndMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void NotifyHorseHiddenForAI(bool bHidden);
	void ResetHorseHiding();
	bool bHideInputHeld = false;
	bool bHideReleaseRequested = false;
};
