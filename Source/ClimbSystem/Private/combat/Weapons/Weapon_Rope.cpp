// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Rope.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "combat/CombatTargetingComponent.h"

AWeapon_Rope::AWeapon_Rope()
{
	PrimaryActorTick.bCanEverTick = true;

	WeaponConfig.WeaponType = EWeaponType::Rope;
	WeaponConfig.WeaponDisplayName = FText::FromString(TEXT("Rope"));
	WeaponConfig.HandSocketName = FName("Hand_R_Socket");
	WeaponConfig.TraceSocketNames.Empty();
	WeaponConfig.bEnableAttackPositioning = false;

	// The rope is carried in the hand and has no separate holster mesh/socket.
	bNoHolster = true;

	RopeSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RopeSpline"));
	RopeSpline->SetupAttachment(RootSceneComponent);
	RopeSpline->SetClosedLoop(false);

	for (int32 Index = 0; Index < MaxRopeSegments; ++Index)
	{
		const FName ComponentName(*FString::Printf(TEXT("RopeSegment_%02d"), Index));
		USplineMeshComponent* Segment = CreateDefaultSubobject<USplineMeshComponent>(ComponentName);
		Segment->SetupAttachment(RopeSpline);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetForwardAxis(ESplineMeshAxis::X, false);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetGenerateOverlapEvents(false);
		Segment->SetVisibility(false);
		RopeSegments.Add(Segment);
	}
}

void AWeapon_Rope::BeginPlay()
{
	Super::BeginPlay();

	// Reuse the existing attack lifecycle, combo state and montage callbacks,
	// while exposing a weapon-specific SwingMontage property to designers.
	WeaponConfig.WeaponType = EWeaponType::Rope;
	WeaponConfig.AttackMontageMode = EAttackMontageMode::SingleComboMontage;
	WeaponConfig.AttackComboMontage = SwingMontage;
	WeaponConfig.bEnableAttackPositioning = false;
	WeaponConfig.TraceSocketNames.Empty();

	if (!IsValid(SwingMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Rope] SwingMontage is not assigned on %s."), *GetName());
	}

	for (USplineMeshComponent* Segment : RopeSegments)
	{
		if (!IsValid(Segment)) continue;
		Segment->SetStaticMesh(RopeSegmentMesh);
		Segment->SetVisibility(false);
	}

	RopeSpline->ClearSplinePoints(false);
	RopeSpline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
	RopeSpline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
	RopeSpline->UpdateSpline();
	ResetRopeVisual();
}

void AWeapon_Rope::AttachToHand(ACharacter* TargetCharacter)
{
	ResetRopeVisual();
	Super::AttachToHand(TargetCharacter);
}

void AWeapon_Rope::AttachToHolster(ACharacter* TargetCharacter)
{
	ResetRopeVisual();
	Super::AttachToHolster(TargetCharacter);
}

void AWeapon_Rope::BeginSwing()
{
	ResetRopeVisual();
	bSwingActive = true;
}

void AWeapon_Rope::ReleaseRope(const UAnimSequenceBase* SourceAnimation)
{
	if (!IsValid(SwingMontage) || SourceAnimation != SwingMontage
		|| !bSwingActive || !GetWorld())
	{
		return;
	}

	FVector Direction;
	if (!ResolveThrowDirection(Direction))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Rope] Could not resolve a valid throw direction."));
		return;
	}

	bSwingActive = false;
	bIsExtending = true;
	RopeStartLocation = GetActorLocation();
	RopeEndLocation = RopeStartLocation;
	RopeDirection = Direction;
	CurrentRopeLength = 0.0f;
	UpdateRopeVisual();

	UE_LOG(LogTemp, Verbose, TEXT("[AWeapon_Rope] Rope released. Max length: %.1f"), MaxRopeLength);
}

bool AWeapon_Rope::ResolveThrowDirection(FVector& OutDirection) const
{
	ACharacter* Character = OwningCharacter.Get();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World)
	{
		return false;
	}

	AActor* Target = nullptr;
	if (const UCombatTargetingComponent* Targeting =
		Character->FindComponentByClass<UCombatTargetingComponent>())
	{
		Target = Targeting->GetCurrentTarget();
	}

	FVector ViewLocation = Character->GetPawnViewLocation();
	FRotator ViewRotation = Character->GetActorRotation();
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else if (AController* Controller = Character->GetController())
	{
		ViewRotation = Controller->GetControlRotation();
	}

	FVector AimLocation = ViewLocation + ViewRotation.Vector() * MaxRopeLength;
	if (IsValid(Target))
	{
		FVector BoundsOrigin;
		FVector BoundsExtent;
		Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);
		AimLocation = BoundsOrigin + FVector(0.0f, 0.0f, BoundsExtent.Z * 0.25f);
	}
	else
	{
		FCollisionQueryParams AimParams(SCENE_QUERY_STAT(RopeAimTrace), true, Character);
		AimParams.AddIgnoredActor(this);
		FHitResult AimHit;
		const FVector AimEnd = ViewLocation + ViewRotation.Vector() * MaxRopeLength;
		if (World->LineTraceSingleByChannel(AimHit, ViewLocation, AimEnd, RopeTraceChannel, AimParams)
			&& AimHit.bBlockingHit)
		{
			AimLocation = AimHit.ImpactPoint;
		}
	}

	OutDirection = (AimLocation - GetActorLocation()).GetSafeNormal();
	if (OutDirection.IsNearlyZero())
	{
		OutDirection = Character->GetActorForwardVector().GetSafeNormal();
	}
	return !OutDirection.IsNearlyZero();
}

void AWeapon_Rope::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsExtending || !GetWorld())
	{
		return;
	}

	const float SafeMaxLength = FMath::Max(0.0f, MaxRopeLength);
	// Keep the rope's root anchored to the animated hand socket as the character moves.
	const FVector CurrentStart = GetActorLocation();
	const FVector HandDelta = CurrentStart - RopeStartLocation;
	RopeStartLocation = CurrentStart;
	RopeEndLocation += HandDelta;

	const float NextLength = FMath::Min(
		SafeMaxLength,
		CurrentRopeLength + FMath::Max(0.0f, ExtensionSpeed) * DeltaSeconds);
	const FVector DesiredEnd = RopeStartLocation + RopeDirection * NextLength;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RopeExtension), true, this);
	if (OwningCharacter.IsValid())
	{
		QueryParams.AddIgnoredActor(OwningCharacter.Get());
	}

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		RopeEndLocation,
		DesiredEnd,
		FQuat::Identity,
		RopeTraceChannel,
		FCollisionShape::MakeSphere(FMath::Max(1.0f, RopeCollisionRadius)),
		QueryParams);

	if (bHit && Hit.bBlockingHit)
	{
		RopeEndLocation = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
		CurrentRopeLength = FVector::Distance(RopeStartLocation, RopeEndLocation);
		bIsExtending = false;
		UE_LOG(LogTemp, Verbose, TEXT("[AWeapon_Rope] Rope extension stopped by hit: %s"), *GetNameSafe(Hit.GetActor()));
	}
	else
	{
		RopeEndLocation = DesiredEnd;
		CurrentRopeLength = NextLength;
		if (CurrentRopeLength >= SafeMaxLength)
		{
			bIsExtending = false;
		}
	}

	UpdateRopeVisual();
}

void AWeapon_Rope::ResetRopeVisual()
{
	bIsExtending = false;
	bSwingActive = false;
	CurrentRopeLength = 0.0f;
	RopeStartLocation = GetActorLocation();
	RopeEndLocation = RopeStartLocation;

	for (USplineMeshComponent* Segment : RopeSegments)
	{
		if (IsValid(Segment))
		{
			Segment->SetVisibility(false);
		}
	}

	if (IsValid(RopeSpline))
	{
		RopeSpline->SetLocationAtSplinePoint(0, FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
		RopeSpline->SetLocationAtSplinePoint(1, FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
		RopeSpline->UpdateSpline();
	}
}

void AWeapon_Rope::UpdateRopeVisual()
{
	if (!IsValid(RopeSpline) || !IsValid(RopeSegmentMesh))
	{
		for (USplineMeshComponent* Segment : RopeSegments)
		{
			if (IsValid(Segment)) Segment->SetVisibility(false);
		}
		return;
	}

	const FVector LocalEnd = GetActorTransform().InverseTransformPosition(RopeEndLocation);
	RopeSpline->SetLocationAtSplinePoint(0, FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
	RopeSpline->SetLocationAtSplinePoint(1, LocalEnd, ESplineCoordinateSpace::Local, false);
	RopeSpline->UpdateSpline();

	const float SplineLength = RopeSpline->GetSplineLength();
	const int32 ActiveSegmentCount = FMath::Clamp(RopeSegmentCount, 1, RopeSegments.Num());
	const bool bShowRope = SplineLength > KINDA_SMALL_NUMBER;

	for (int32 Index = 0; Index < RopeSegments.Num(); ++Index)
	{
		USplineMeshComponent* Segment = RopeSegments[Index];
		if (!IsValid(Segment)) continue;

		if (!bShowRope || Index >= ActiveSegmentCount)
		{
			Segment->SetVisibility(false);
			continue;
		}

		const float StartDistance = SplineLength * (static_cast<float>(Index) / ActiveSegmentCount);
		const float EndDistance = SplineLength * (static_cast<float>(Index + 1) / ActiveSegmentCount);
		const FVector StartPosition = RopeSpline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
		const FVector EndPosition = RopeSpline->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);
		const FVector StartTangent = RopeSpline->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
		const FVector EndTangent = RopeSpline->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);

		Segment->SetStartAndEnd(StartPosition, StartTangent, EndPosition, EndTangent, true);
		Segment->SetVisibility(true);
	}
}
