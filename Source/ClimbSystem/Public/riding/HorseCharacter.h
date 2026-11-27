// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "riding/RidingTypes.h"
#include "HorseCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class URidingComponent;
class UCombatComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class CLIMBSYSTEM_API AHorseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHorseCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Horse|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Horse|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Riding")
	FName RiderAttachSocketName = TEXT("Rider_Socket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Riding")
	FTransform LeftMountOffset = FTransform(FRotator::ZeroRotator, FVector(0.0f, -110.0f, 0.0f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Riding")
	FTransform RightMountOffset = FTransform(FRotator::ZeroRotator, FVector(0.0f, 110.0f, 0.0f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Riding")
	FTransform RearMountOffset = FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(-150.0f, 0.0f, 0.0f));


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Riding")
	FVector LeftDismountOffset = FVector(0.0f, -180.0f, 35.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Riding")
	FVector RightDismountOffset = FVector(0.0f, 180.0f, 35.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Riding")
	FVector RearDismountOffset = FVector(-220.0f, 0.0f, 35.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 850.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Movement", meta = (ClampMin = "0.0"))
	float ReverseSpeed = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Movement", meta = (ClampMin = "0.0"))
	float TurnRate = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Movement")
	bool bAllowReverseMovement = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Movement")
	bool bAllowStrafeMovement = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> DismountAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> NextWeaponAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> GuardAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|Input")
	TObjectPtr<UInputAction> ParryAction;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Horse|Riding")
	TObjectPtr<ACharacter> CurrentRider = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Horse|Riding")
	ERidingState RidingState = ERidingState::Unmounted;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Horse|Movement")
	bool bIsSprinting = false;

	UFUNCTION(BlueprintPure, Category = "Horse|Riding")
	bool CanAcceptRider() const;

	UFUNCTION(BlueprintPure, Category = "Horse|Riding")
	FTransform GetMountWorldTransform(EHorseMountDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "Horse|Riding")
	FTransform GetDismountWorldTransform(EHorseMountDirection Direction) const;

	UFUNCTION(BlueprintCallable, Category = "Horse|Riding")
	void SetCurrentRider(ACharacter* Rider);

	UFUNCTION(BlueprintCallable, Category = "Horse|Riding")
	void SetRidingState(ERidingState NewState);

	UFUNCTION(BlueprintCallable, Category = "Horse|Movement")
	void SetSprinting(bool bEnable);

	void SetRiderInputActions(UInputAction* Move, UInputAction* Look, UInputAction* Sprint,
		UInputAction* Dismount, UInputAction* Attack, UInputAction* NextWeapon,
		UInputAction* Guard, UInputAction* Parry);

	UFUNCTION(BlueprintPure, Category = "Horse|Movement")
	float GetCurrentHorseSpeed() const;

protected:
	virtual void BeginPlay() override;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSprintStarted(const FInputActionValue& Value);
	void OnSprintEnded(const FInputActionValue& Value);
	void OnDismount(const FInputActionValue& Value);
	void OnAttackStarted(const FInputActionValue& Value);
	void OnAttackReleased(const FInputActionValue& Value);
	void OnNextWeapon(const FInputActionValue& Value);
	void OnGuardStarted(const FInputActionValue& Value);
	void OnGuardEnded(const FInputActionValue& Value);
	void OnParry(const FInputActionValue& Value);

private:
	TWeakObjectPtr<UCombatComponent> RiderCombatComponent;
	bool bMountedRopeAimInputHeld = false;
	void OnAttackCanceled(const FInputActionValue& Value);
};
