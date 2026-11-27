// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HorseAnimInstance.generated.h"

class AHorseCharacter;

UCLASS(Transient, Blueprintable, BlueprintType)
class CLIMBSYSTEM_API UHorseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	float ForwardSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	float TurnAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	float Acceleration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Locomotion")
	bool bSideStack = false;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Body")
	FRotator ChestRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Body")
	FRotator SpineRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Body")
	FRotator HipRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Body")
	FRotator LeftLegRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Body")
	FRotator RightLegRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|Body")
	FRotator TiltRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|IK")
	FVector FrontLeftIK = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|IK")
	FVector FrontRightIK = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|IK")
	FVector BackLeftIK = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|IK")
	FVector BackRightIK = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|IK")
	float FrontLegIKAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Horse|IK")
	float BackLegIKAlpha = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|IK")
	FName FrontLeftFootBone = TEXT("ik_foot_front_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|IK")
	FName FrontRightFootBone = TEXT("ik_foot_front_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|IK")
	FName BackLeftFootBone = TEXT("ik_foot_back_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|IK")
	FName BackRightFootBone = TEXT("ik_foot_back_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|IK", meta = (ClampMin = "0.0"))
	float IKTraceUp = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|IK", meta = (ClampMin = "0.0"))
	float IKTraceDown = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horse|IK", meta = (ClampMin = "0.0"))
	float IKInterpSpeed = 12.0f;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	TWeakObjectPtr<AHorseCharacter> CachedHorse;
	FVector FrontLeftIKTarget = FVector::ZeroVector;
	FVector FrontRightIKTarget = FVector::ZeroVector;
	FVector BackLeftIKTarget = FVector::ZeroVector;
	FVector BackRightIKTarget = FVector::ZeroVector;

	void UpdateFootIK(float DeltaSeconds, FName BoneName, FVector& CurrentOffset, FVector& TargetOffset);
};
