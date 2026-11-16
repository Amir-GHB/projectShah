// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatPositioningComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UCombatPositioningComponent::UCombatPositioningComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombatPositioningComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningCharacter = Cast<ACharacter>(GetOwner());
	SetComponentTickEnabled(false);
}

void UCombatPositioningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndAttackPositioning();
	Super::EndPlay(EndPlayReason);
}

void UCombatPositioningComponent::BeginAttackPositioning(
	AActor* Target,
	float InPreferredDistance,
	float InDistanceTolerance,
	float InMaxEngagementRange,
	bool bEnablePositioning)
{
	// A new combo step replaces the previous step's positioning operation.
	EndAttackPositioning();

	if (!bEnablePositioning || !OwningCharacter.IsValid() || !IsValid(Target) ||
		Target == OwningCharacter.Get() || InPreferredDistance <= 0.0f ||
		InMaxEngagementRange <= 0.0f)
	{
		return;
	}

	CombatTarget = Target;
	PreferredDistance = FMath::Max(0.0f, InPreferredDistance);
	DistanceTolerance = FMath::Max(0.0f, InDistanceTolerance);
	MaxEngagementRange = FMath::Max(0.0f, InMaxEngagementRange);
	bAttackPositioningActive = true;

	if (UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement())
	{
		bPreviousOrientRotationToMovement = Movement->bOrientRotationToMovement;
		if (bRotateToTargetDuringAttack)
		{
			Movement->bOrientRotationToMovement = false;
		}
	}

	SetComponentTickEnabled(true);
}

void UCombatPositioningComponent::EndAttackPositioning()
{
	if (!bAttackPositioningActive)
	{
		CombatTarget.Reset();
		SetComponentTickEnabled(false);
		return;
	}

	bAttackPositioningActive = false;
	CombatTarget.Reset();
	SetComponentTickEnabled(false);
	RestoreMovementFacing();
}

void UCombatPositioningComponent::RestoreMovementFacing()
{
	if (OwningCharacter.IsValid())
	{
		if (UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bPreviousOrientRotationToMovement;
		}
	}
}

void UCombatPositioningComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = OwningCharacter.Get();
	AActor* Target = CombatTarget.Get();
	if (!bAttackPositioningActive || !IsValid(Character) || !IsValid(Target) || Target == Character)
	{
		EndAttackPositioning();
		return;
	}

	const FVector ToTarget = FVector::VectorPlaneProject(
		Target->GetActorLocation() - Character->GetActorLocation(), FVector::UpVector);
	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER || Distance > MaxEngagementRange)
	{
		return;
	}

	if (bRotateToTargetDuringAttack)
	{
		const FRotator CurrentRotation = Character->GetActorRotation();
		const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
		const FRotator NewRotation = FMath::RInterpTo(
			CurrentRotation, DesiredRotation, DeltaTime, RotationInterpSpeed);
		Character->SetActorRotation(FRotator(CurrentRotation.Pitch, NewRotation.Yaw, CurrentRotation.Roll));
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		return;
	}

	// AddMovementInput is additive to player input and remains collision-aware through CharacterMovement.
	// Positive error approaches; negative error retreats. The tolerance band prevents oscillation.
	const float DistanceError = Distance - PreferredDistance;
	const float AbsoluteError = FMath::Abs(DistanceError);
	if (AbsoluteError <= DistanceTolerance)
	{
		return;
	}

	const FVector Direction = ToTarget.GetSafeNormal() * (DistanceError > 0.0f ? 1.0f : -1.0f);
	const float CorrectionAlpha = FMath::Clamp(AbsoluteError / 120.0f, 0.15f, 1.0f);
	const float InputScale = FMath::Clamp(MaxCorrectionInput, 0.0f, 1.0f) * CorrectionAlpha;
	Character->AddMovementInput(Direction, InputScale);
}
