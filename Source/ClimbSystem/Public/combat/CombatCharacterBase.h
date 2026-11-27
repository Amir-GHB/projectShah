// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatCharacterBase.generated.h"

class UCombatComponent;
class UCombatTargetingComponent;
class UCombatPositioningComponent;
class UDamageHandlerComponent;
class UDamageReactionComponent;
class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UUserWidget;
class UMotionWarpingComponent;
class URidingComponent;
class AHorseCharacter;
struct FInputActionValue;

UCLASS()
class CLIMBSYSTEM_API ACombatCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ACombatCharacterBase();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UDamageHandlerComponent> DamageHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UCombatTargetingComponent> CombatTargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UCombatPositioningComponent> CombatPositioningComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UDamageReactionComponent> DamageReactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Motion Warping")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Riding")
	TObjectPtr<URidingComponent> RidingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Camera shoulder offset while aiming. Positive Y moves toward the character's right shoulder; negative Y selects the left. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Aim")
	FVector AimCameraSocketOffset = FVector(0.0f, 55.0f, 30.0f);

	/** Spring-arm length while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Aim", meta = (ClampMin = "0.0"))
	float AimCameraArmLength = 220.0f;

	/** Interpolation speed used when entering and leaving aim camera mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Aim", meta = (ClampMin = "0.1"))
	float AimCameraInterpSpeed = 8.0f;

	/** Optional crosshair widget displayed while aim camera mode is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Aim")
	TSubclassOf<UUserWidget> AimCrosshairWidgetClass;

	/** Enters the shared shoulder-camera aiming mode. Safe to reuse for other weapons. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Aim")
	void BeginAimCameraMode();

	/** Leaves aim mode, removes the crosshair and smoothly restores the previous camera settings. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Aim")
	void EndAimCameraMode();

	void ConfigureHorseInput(AHorseCharacter* Horse) const;

	/** Hides only the aiming reticle while leaving the shoulder camera active. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Aim")
	void HideAimCrosshair();

	UFUNCTION(BlueprintPure, Category = "Camera|Aim")
	bool IsAimCameraModeActive() const { return bAimCameraModeActive; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	/** Held input for entering/leaving the sustained guard stance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GuardAction;

	/** Press input for a weapon-specific parry attempt while guarding. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ParryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> NextWeaponAction;

	/** Optional input action for setting/clearing the preferred semi-lock target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> TargetLockAction;

	/** Test interaction: throw a nearby throwable environmental actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ThrowNearbyActorAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Riding")
	TObjectPtr<UInputAction> MountAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Riding")
	TObjectPtr<UInputAction> SprintAction;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpEnded(const FInputActionValue& Value);
	void OnAttackTriggered(const FInputActionValue& Value);
	void OnAttackReleased(const FInputActionValue& Value);
	void OnAttackCanceled(const FInputActionValue& Value);
	void OnGuardStarted(const FInputActionValue& Value);
	void OnGuardEnded(const FInputActionValue& Value);
	void OnParryTriggered(const FInputActionValue& Value);
	void OnNextWeaponTriggered(const FInputActionValue& Value);
	void OnTargetLockTriggered(const FInputActionValue& Value);
	void OnThrowNearbyActorTriggered(const FInputActionValue& Value);
	void OnMountTriggered(const FInputActionValue& Value);

	bool bRopeAimInputHeld = false;
	bool bAimCameraModeActive = false;
	bool bAimCameraTransitionActive = false;
	float DefaultCameraArmLength = 320.0f;
	FVector DefaultCameraSocketOffset = FVector::ZeroVector;
	TWeakObjectPtr<USpringArmComponent> ActiveAimCameraBoom;
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> AimCrosshairWidget = nullptr;

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
};
