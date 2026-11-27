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
};
