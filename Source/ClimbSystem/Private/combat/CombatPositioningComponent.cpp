// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatPositioningComponent.h"

#include "MotionWarpingComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "GameFramework/Character.h"

UCombatPositioningComponent::UCombatPositioningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatPositioningComponent::BeginAttackPositioning(
	AActor* Target,
	FName WarpTargetName,
	float InPreferredDistance,
	float InMaxEngagementRange,
	bool bEnablePositioning)
{
	// A new combo step replaces the previous step's target transform.
	EndAttackPositioning();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!bEnablePositioning || !IsValid(Character) || !IsValid(Target) ||
		Target == Character || WarpTargetName.IsNone() ||
		InPreferredDistance <= 0.0f || InMaxEngagementRange <= 0.0f)
	{
		return;
	}

	if (const UDamageHandlerComponent* TargetDamage =
		Target->FindComponentByClass<UDamageHandlerComponent>())
	{
		if (TargetDamage->bIsDead)
		{
			return;
		}
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	const FVector ToTarget = FVector::VectorPlaneProject(
		Target->GetActorLocation() - CharacterLocation, FVector::UpVector);
	const float CurrentDistance = ToTarget.Size();

	if (CurrentDistance <= KINDA_SMALL_NUMBER || CurrentDistance > InMaxEngagementRange)
	{
		return;
	}

	UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>();
	if (!MotionWarping)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CombatPositioning] %s has no MotionWarpingComponent; attack warp skipped."),
			*GetNameSafe(Character));
		return;
	}

	// Place the attacker at the configured distance from the target.
	// Z is preserved so ground-height differences do not pull the capsule vertically.
	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	const FVector DesiredLocation = FVector(
		Target->GetActorLocation().X - DirectionToTarget.X * InPreferredDistance,
		Target->GetActorLocation().Y - DirectionToTarget.Y * InPreferredDistance,
		CharacterLocation.Z);

	// The final character orientation faces the target, independently of the translation.
	const FRotator DesiredRotation(0.0f, DirectionToTarget.Rotation().Yaw, 0.0f);
	PreparedWarpTransform = FTransform(DesiredRotation, DesiredLocation, FVector::OneVector);

	MotionWarping->AddOrUpdateWarpTargetFromTransform(WarpTargetName, PreparedWarpTransform);
	MotionWarpingComponent = MotionWarping;
	ActiveWarpTargetName = WarpTargetName;
	bAttackPositioningActive = true;
}

void UCombatPositioningComponent::EndAttackPositioning()
{
	if (MotionWarpingComponent.IsValid() && !ActiveWarpTargetName.IsNone())
	{
		MotionWarpingComponent->RemoveWarpTarget(ActiveWarpTargetName);
	}

	MotionWarpingComponent.Reset();
	ActiveWarpTargetName = NAME_None;
	PreparedWarpTransform = FTransform::Identity;
	bAttackPositioningActive = false;
}
