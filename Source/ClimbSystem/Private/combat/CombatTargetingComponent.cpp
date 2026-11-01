// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatTargetingComponent.h"

#include "combat/DamageHandlerComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UCombatTargetingComponent::UCombatTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombatTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningCharacter = Cast<ACharacter>(GetOwner());
	SetComponentTickEnabled(false);
}

void UCombatTargetingComponent::BeginAttackFocus()
{
	if (bAttackFocusActive || !OwningCharacter.IsValid())
	{
		return;
	}

	bAttackFocusActive = true;
	bCorrectionApplied = false;
	CorrectionTimeRemaining = 0.0f;
	TargetRefreshElapsed = TargetRefreshInterval;

	if (UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement())
	{
		bPreviousOrientRotationToMovement = Movement->bOrientRotationToMovement;
	}

	AActor* InitialTarget = IsValidTarget(ManualTarget.Get())
		&& IsWithinFocusRange(ManualTarget.Get())
		&& (!bRequireLineOfSight || HasLineOfSight(ManualTarget.Get()))
		? ManualTarget.Get()
		: FindBestTarget();

	SetCurrentTarget(InitialTarget);
	SetComponentTickEnabled(true);
}

void UCombatTargetingComponent::EndAttackFocus()
{
	if (!bAttackFocusActive)
	{
		return;
	}

	bAttackFocusActive = false;
	CurrentTarget.Reset();
	CorrectionTimeRemaining = 0.0f;
	TargetRefreshElapsed = 0.0f;

	if (OwningCharacter.IsValid())
	{
		if (UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bPreviousOrientRotationToMovement;
		}
	}

	SetComponentTickEnabled(false);
}

void UCombatTargetingComponent::ToggleManualLock()
{
	if (IsValidTarget(ManualTarget.Get()))
	{
		ClearManualTarget();
		return;
	}

	AActor* Candidate = FindBestTarget();
	if (IsValidTarget(Candidate))
	{
		ManualTarget = Candidate;
		if (bAttackFocusActive)
		{
			SetCurrentTarget(Candidate);
		}
	}
}

void UCombatTargetingComponent::SetManualTarget(AActor* NewTarget)
{
	if (IsValidTarget(NewTarget))
	{
		ManualTarget = NewTarget;
		if (bAttackFocusActive && IsWithinFocusRange(NewTarget)
			&& (!bRequireLineOfSight || HasLineOfSight(NewTarget)))
		{
			SetCurrentTarget(NewTarget);
		}
	}
	else
	{
		ClearManualTarget();
	}
}

void UCombatTargetingComponent::ClearManualTarget()
{
	ManualTarget.Reset();
	if (bAttackFocusActive)
	{
		SetCurrentTarget(FindBestTarget());
	}
}

bool UCombatTargetingComponent::IsValidTarget(AActor* Candidate) const
{
	if (!IsValid(Candidate) || Candidate == GetOwner())
	{
		return false;
	}

	const UDamageHandlerComponent* DamageHandler =
		Candidate->FindComponentByClass<UDamageHandlerComponent>();

	return DamageHandler && !DamageHandler->bIsDead;
}

bool UCombatTargetingComponent::IsWithinFocusRange(const AActor* Candidate) const
{
	return IsValid(Candidate) && OwningCharacter.IsValid()
		&& FVector::DistSquared(OwningCharacter->GetActorLocation(), Candidate->GetActorLocation())
			<= FMath::Square(FMath::Max(0.0f, MaxFocusRange));
}

bool UCombatTargetingComponent::HasLineOfSight(const AActor* Candidate) const
{
	if (!IsValid(Candidate) || !OwningCharacter.IsValid() || !GetWorld())
	{
		return false;
	}

	FVector ViewLocation = OwningCharacter->GetPawnViewLocation();
	FRotator ViewRotation = OwningCharacter->GetActorRotation();
	if (const APlayerController* PC = Cast<APlayerController>(OwningCharacter->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector TargetLocation = Candidate->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatTargetVisibility), true, OwningCharacter.Get());
	Params.AddIgnoredActor(Candidate);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, ViewLocation, TargetLocation, ECC_Visibility, Params);

	return !bHit || Hit.GetActor() == Candidate;
}

AActor* UCombatTargetingComponent::FindBestTarget() const
{
	if (!OwningCharacter.IsValid() || !GetWorld())
	{
		return nullptr;
	}

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CombatTargetCandidates), false, OwningCharacter.Get());
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		OwningCharacter->GetActorLocation(),
		FQuat::Identity,
		ObjectTypes,
		FCollisionShape::MakeSphere(FMath::Max(0.0f, MaxFocusRange)),
		QueryParams);

	FVector ViewLocation = OwningCharacter->GetPawnViewLocation();
	FRotator ViewRotation = OwningCharacter->GetActorRotation();
	APlayerController* PlayerController = Cast<APlayerController>(OwningCharacter->GetController());
	if (PlayerController)
	{
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	if (PlayerController)
	{
		PlayerController->GetViewportSize(ViewportX, ViewportY);
	}

	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;
	TSet<TWeakObjectPtr<AActor>> ConsideredActors;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const TWeakObjectPtr<AActor> CandidatePtr(Candidate);
		if (!IsValidTarget(Candidate) || !IsWithinFocusRange(Candidate)
			|| ConsideredActors.Contains(CandidatePtr))
		{
			continue;
		}
		ConsideredActors.Add(CandidatePtr);

		if (bRequireLineOfSight && !HasLineOfSight(Candidate))
		{
			continue;
		}

		const FVector ToTarget = Candidate->GetActorLocation() - ViewLocation;
		const float Distance = ToTarget.Size();
		if (Distance <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float ViewAlignment = FMath::Clamp(
			FVector::DotProduct(ViewDirection, ToTarget / Distance), 0.0f, 1.0f);
		const float DistanceScore = 1.0f - FMath::Clamp(Distance / MaxFocusRange, 0.0f, 1.0f);

		float ScreenCenterScore = ViewAlignment;
		if (PlayerController && ViewportX > 0 && ViewportY > 0)
		{
			FVector2D ScreenPosition;
			if (PlayerController->ProjectWorldLocationToScreen(Candidate->GetActorLocation(), ScreenPosition))
			{
				const FVector2D ScreenCenter(ViewportX * 0.5f, ViewportY * 0.5f);
				const float MaxScreenDistance = FVector2D(ViewportX, ViewportY).Size() * 0.5f;
				ScreenCenterScore = 1.0f - FMath::Clamp(
					FVector2D::Distance(ScreenPosition, ScreenCenter) / MaxScreenDistance, 0.0f, 1.0f);
			}
			else
			{
				ScreenCenterScore = 0.0f;
			}
		}

		const float RetentionBonus = Candidate == CurrentTarget.Get() ? 0.12f : 0.0f;
		const float Score = ViewAlignment * 0.45f
			+ ScreenCenterScore * 0.30f
			+ DistanceScore * 0.25f
			+ RetentionBonus;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

void UCombatTargetingComponent::SetCurrentTarget(AActor* NewTarget)
{
	if (NewTarget && (!IsValidTarget(NewTarget) || !IsWithinFocusRange(NewTarget)))
	{
		NewTarget = nullptr;
	}

	CurrentTarget = NewTarget;

	if (UCharacterMovementComponent* Movement =
		OwningCharacter.IsValid() ? OwningCharacter->GetCharacterMovement() : nullptr)
	{
		Movement->bOrientRotationToMovement = NewTarget
			? false : bPreviousOrientRotationToMovement;
	}
}

void UCombatTargetingComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bAttackFocusActive || !OwningCharacter.IsValid())
	{
		EndAttackFocus();
		return;
	}

	TargetRefreshElapsed += DeltaTime;
	AActor* Target = CurrentTarget.Get();
	if (!IsValidTarget(Target) || !IsWithinFocusRange(Target)
		|| (bRequireLineOfSight && !HasLineOfSight(Target)))
	{
		if (TargetRefreshElapsed >= TargetRefreshInterval)
		{
			TargetRefreshElapsed = 0.0f;
			Target = IsValidTarget(ManualTarget.Get())
				&& IsWithinFocusRange(ManualTarget.Get())
				&& (!bRequireLineOfSight || HasLineOfSight(ManualTarget.Get()))
				? ManualTarget.Get()
				: FindBestTarget();
			SetCurrentTarget(Target);
		}
	}

	Target = CurrentTarget.Get();
	if (!IsValidTarget(Target) || !IsWithinFocusRange(Target))
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - OwningCharacter->GetActorLocation();
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	const FRotator CurrentRotation = OwningCharacter->GetActorRotation();
	const FRotator NewRotation = FMath::RInterpTo(
		CurrentRotation, DesiredRotation, DeltaTime, RotationInterpSpeed);
	OwningCharacter->SetActorRotation(FRotator(CurrentRotation.Pitch, NewRotation.Yaw, CurrentRotation.Roll));

	// A single, brief, collision-aware movement-input nudge per attack. It is deliberately
	// omitted at long range, while airborne, or when the character is already moving quickly.
	if (!bCorrectionApplied)
	{
		const float Distance2D = FVector::Dist2D(
			OwningCharacter->GetActorLocation(), Target->GetActorLocation());
		UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement();
		if (Movement && Movement->IsMovingOnGround()
			&& Distance2D >= CorrectionStartDistance
			&& Distance2D <= CorrectionMaxDistance
			&& Movement->Velocity.Size2D() < 180.0f
			&& CorrectionDuration > 0.0f)
		{
			CorrectionTimeRemaining = CorrectionDuration;
			bCorrectionApplied = true;
		}
	}

	if (CorrectionTimeRemaining > 0.0f)
	{
		const FVector CorrectionDirection = FVector::VectorPlaneProject(
			Target->GetActorLocation() - OwningCharacter->GetActorLocation(),
			FVector::UpVector).GetSafeNormal();
		OwningCharacter->AddMovementInput(
			CorrectionDirection,
			FMath::Clamp(CorrectionStrength, 0.0f, 1.0f));
		CorrectionTimeRemaining = FMath::Max(0.0f, CorrectionTimeRemaining - DeltaTime);
	}
}
