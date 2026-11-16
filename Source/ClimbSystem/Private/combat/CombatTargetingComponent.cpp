// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatTargetingComponent.h"

#include "combat/DamageHandlerComponent.h"
#include "CombatDamageable.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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
	TargetRefreshElapsed = TargetRefreshInterval;

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
	TargetRefreshElapsed = 0.0f;

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
	if (DamageHandler)
	{
		return !DamageHandler->bIsDead;
	}

	// The repository also contains the original Variant_Combat actors, which expose
	// ICombatDamageable and route hits through TakeDamage rather than this component.
	return Cast<ICombatDamageable>(Candidate) != nullptr;
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
		if (ViewAlignment <= 0.0f)
		{
			continue;
		}
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
	const bool bTargetInvalid = !IsValidTarget(Target) || !IsWithinFocusRange(Target);
	const bool bRefreshDue = TargetRefreshElapsed >= TargetRefreshInterval;
	bool bNeedsReplacement = bTargetInvalid;

	// Visibility is intentionally checked at the refresh cadence, not every frame.
	if (!bNeedsReplacement && bRefreshDue && bRequireLineOfSight && !HasLineOfSight(Target))
	{
		bNeedsReplacement = true;
	}

	if (bNeedsReplacement)
	{
		SetCurrentTarget(nullptr);
		if (bRefreshDue)
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
	else if (bRefreshDue)
	{
		TargetRefreshElapsed = 0.0f;
	}
	// This component deliberately stops here: it owns target acquisition and validation only.
}
