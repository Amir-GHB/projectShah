// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "riding/HorseAnimInstance.h"
#include "riding/HorseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UHorseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CachedHorse = Cast<AHorseCharacter>(TryGetPawnOwner());
}

void UHorseAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	AHorseCharacter* Horse = CachedHorse.Get();
	if (!IsValid(Horse))
	{
		CachedHorse = Cast<AHorseCharacter>(TryGetPawnOwner());
		Horse = CachedHorse.Get();
	}
	if (!IsValid(Horse) || !Horse->GetMesh())
	{
		GroundSpeed = ForwardSpeed = TurnAlpha = 0.0f;
		bIsMoving = bIsSprinting = bSideStack = false;
		FrontLegIKAlpha = BackLegIKAlpha = 0.0f;
		return;
	}

	const FVector Velocity = Horse->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	ForwardSpeed = FVector::DotProduct(Velocity, Horse->GetActorForwardVector());
	bIsMoving = GroundSpeed > 5.0f;
	bIsSprinting = Horse->bIsSprinting;

	float YawError = 0.0f;
	if (AController* Controller = Horse->GetController())
	{
		YawError = FMath::FindDeltaAngleDegrees(Horse->GetActorRotation().Yaw, Controller->GetControlRotation().Yaw);
	}
	TurnAlpha = FMath::Clamp(YawError / 75.0f, -1.0f, 1.0f);
	bSideStack = GroundSpeed < 15.0f && FMath::Abs(YawError) > 12.0f;

	const float Turn = TurnAlpha;
	ChestRotation = FRotator(0.0f, Turn * 16.0f, 0.0f);
	SpineRotation = FRotator(0.0f, Turn * 11.0f, 0.0f);
	HipRotation = FRotator(0.0f, -Turn * 8.0f, 0.0f);
	LeftLegRotation = FRotator(0.0f, -FMath::Max(0.0f, Turn) * 7.0f, 0.0f);
	RightLegRotation = FRotator(0.0f, FMath::Max(0.0f, -Turn) * 7.0f, 0.0f);
	TiltRotation = FRotator(0.0f, 0.0f, -Turn * 9.0f);

	const bool bGrounded = Horse->GetCharacterMovement() && !Horse->GetCharacterMovement()->IsFalling();
	if (bGrounded)
	{
		UpdateFootIK(DeltaSeconds, FrontLeftFootBone, FrontLeftIK, FrontLeftIKTarget);
		UpdateFootIK(DeltaSeconds, FrontRightFootBone, FrontRightIK, FrontRightIKTarget);
		UpdateFootIK(DeltaSeconds, BackLeftFootBone, BackLeftIK, BackLeftIKTarget);
		UpdateFootIK(DeltaSeconds, BackRightFootBone, BackRightIK, BackRightIKTarget);
		FrontLegIKAlpha = FMath::FInterpTo(FrontLegIKAlpha, 1.0f, DeltaSeconds, IKInterpSpeed);
		BackLegIKAlpha = FMath::FInterpTo(BackLegIKAlpha, 1.0f, DeltaSeconds, IKInterpSpeed);
	}
	else
	{
		FrontLeftIK = FMath::VInterpTo(FrontLeftIK, FVector::ZeroVector, DeltaSeconds, IKInterpSpeed);
		FrontRightIK = FMath::VInterpTo(FrontRightIK, FVector::ZeroVector, DeltaSeconds, IKInterpSpeed);
		BackLeftIK = FMath::VInterpTo(BackLeftIK, FVector::ZeroVector, DeltaSeconds, IKInterpSpeed);
		BackRightIK = FMath::VInterpTo(BackRightIK, FVector::ZeroVector, DeltaSeconds, IKInterpSpeed);
		FrontLegIKAlpha = FMath::FInterpTo(FrontLegIKAlpha, 0.0f, DeltaSeconds, IKInterpSpeed);
		BackLegIKAlpha = FMath::FInterpTo(BackLegIKAlpha, 0.0f, DeltaSeconds, IKInterpSpeed);
	}
}

void UHorseAnimInstance::UpdateFootIK(float DeltaSeconds, FName BoneName, FVector& CurrentOffset, FVector& TargetOffset)
{
	AHorseCharacter* Horse = CachedHorse.Get();
	if (!IsValid(Horse) || !Horse->GetMesh() || BoneName.IsNone()) return;

	const FVector FootLocation = Horse->GetMesh()->GetBoneLocation(BoneName);
	if (FootLocation.IsNearlyZero()) return;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HorseFootIK), false, Horse);
	const FVector Start = FootLocation + FVector::UpVector * IKTraceUp;
	const FVector End = FootLocation - FVector::UpVector * IKTraceDown;
	if (Horse->GetWorld() && Horse->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.bBlockingHit)
	{
		TargetOffset = FVector(0.0f, 0.0f, Hit.ImpactPoint.Z - FootLocation.Z);
	}
	else
	{
		TargetOffset = FVector::ZeroVector;
	}
	CurrentOffset = FMath::VInterpTo(CurrentOffset, TargetOffset, DeltaSeconds, IKInterpSpeed);
}
