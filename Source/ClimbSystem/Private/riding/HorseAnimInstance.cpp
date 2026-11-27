// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "riding/HorseAnimInstance.h"
#include "riding/HorseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
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
		bIsMoving = bIsSprinting = bSideStack = bIsAccelerating = false;
		Acceleration = 0.0f;
		FrontLegIKAlpha = BackLegIKAlpha = 0.0f;
		return;
	}

	const FVector Velocity = Horse->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	ForwardSpeed = FVector::DotProduct(Velocity, Horse->GetActorForwardVector());
	bIsMoving = GroundSpeed > 5.0f;
	bIsSprinting = Horse->bIsSprinting;
	if (const UCharacterMovementComponent* Movement = Horse->GetCharacterMovement())
	{
		Acceleration = Movement->GetCurrentAcceleration().Size2D();
		bIsAccelerating = Acceleration > 5.0f;
	}

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
	const USkeletalMeshComponent* HorseMesh = Horse->GetMesh();
	const bool bFrontBonesValid = HorseMesh->GetBoneIndex(FrontLeftFootBone) != INDEX_NONE
		&& HorseMesh->GetBoneIndex(FrontRightFootBone) != INDEX_NONE;
	const bool bBackBonesValid = HorseMesh->GetBoneIndex(BackLeftFootBone) != INDEX_NONE
		&& HorseMesh->GetBoneIndex(BackRightFootBone) != INDEX_NONE;

	if (bGrounded && bFrontBonesValid)
	{
		UpdateFootIK(DeltaSeconds, FrontLeftFootBone, FrontLeftIK, FrontLeftIKTarget);
		UpdateFootIK(DeltaSeconds, FrontRightFootBone, FrontRightIK, FrontRightIKTarget);
	}
	if (bGrounded && bBackBonesValid)
	{
		UpdateFootIK(DeltaSeconds, BackLeftFootBone, BackLeftIK, BackLeftIKTarget);
		UpdateFootIK(DeltaSeconds, BackRightFootBone, BackRightIK, BackRightIKTarget);
	}
	FrontLegIKAlpha = FMath::FInterpTo(FrontLegIKAlpha,
		(bGrounded && bFrontBonesValid) ? 1.0f : 0.0f, DeltaSeconds, IKInterpSpeed);
	BackLegIKAlpha = FMath::FInterpTo(BackLegIKAlpha,
		(bGrounded && bBackBonesValid) ? 1.0f : 0.0f, DeltaSeconds, IKInterpSpeed);
}

void UHorseAnimInstance::UpdateFootIK(float DeltaSeconds, FName BoneName, FVector& CurrentOffset, FVector& TargetOffset)
{
	AHorseCharacter* Horse = CachedHorse.Get();
	if (!IsValid(Horse) || !Horse->GetMesh() || BoneName.IsNone()) return;

	USkeletalMeshComponent* Mesh = Horse->GetMesh();
	if (Mesh->GetBoneIndex(BoneName) == INDEX_NONE) return;

	const FVector FootLocation = Mesh->GetBoneLocation(BoneName);
	FVector TargetWorldLocation = FootLocation;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HorseFootIK), false, Horse);
	const FVector Start = FootLocation + FVector::UpVector * IKTraceUp;
	const FVector End = FootLocation - FVector::UpVector * IKTraceDown;
	if (Horse->GetWorld() && Horse->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.bBlockingHit)
	{
		TargetWorldLocation.Z = Hit.ImpactPoint.Z;
	}

	// The Two Bone IK nodes should consume these values in Component Space.
	TargetOffset = Mesh->GetComponentTransform().InverseTransformPosition(TargetWorldLocation);
	CurrentOffset = FMath::VInterpTo(CurrentOffset, TargetOffset, DeltaSeconds, IKInterpSpeed);
}
