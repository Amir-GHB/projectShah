// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Rope.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
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

	WeaponConfig.WeaponType = EWeaponType::Rope;
	WeaponConfig.AttackMontageMode = EAttackMontageMode::SingleComboMontage;
	WeaponConfig.AttackComboMontage = CharacterSwingMontage;
	WeaponConfig.bEnableAttackPositioning = false;
	WeaponConfig.TraceSocketNames.Empty();

	if (!IsValid(CharacterSwingMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Rope] CharacterSwingMontage is not assigned on %s."), *GetName());
	}
	if (!IsValid(RopeSegmentMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Rope] RopeSegmentMesh is not assigned on %s; the rope will be invisible."), *GetName());
	}

	for (USplineMeshComponent* Segment : RopeSegments)
	{
		if (!IsValid(Segment)) continue;
		Segment->SetStaticMesh(RopeSegmentMesh);
		Segment->SetVisibility(false);
	}

	ResetRopeVisual();
}

void AWeapon_Rope::AttachToHand(ACharacter* TargetCharacter)
{
	ResetRopeVisual();
	Super::AttachToHand(TargetCharacter);
	LastAnchorLocation = GetActorLocation();
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
	LastAnchorLocation = GetActorLocation();
}

void AWeapon_Rope::ReleaseRope(const UAnimSequenceBase* SourceAnimation)
{
	if (!IsValid(CharacterSwingMontage) || SourceAnimation != CharacterSwingMontage
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
	InitializeRopeSimulation(Direction);
	UE_LOG(LogTemp, Verbose, TEXT("[AWeapon_Rope] Rope physically thrown. Max length: %.1f"), MaxRopeLength);
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

void AWeapon_Rope::InitializeRopeSimulation(const FVector& Direction)
{
	const int32 SegmentCount = FMath::Clamp(RopeSegmentCount, 1, MaxRopeSegments);
	const int32 ParticleCount = SegmentCount + 1;
	const FVector Anchor = GetActorLocation();

	ParticlePositions.Init(Anchor, ParticleCount);
	PreviousParticlePositions.Init(Anchor, ParticleCount);

	RopeStartLocation = Anchor;
	LastAnchorLocation = Anchor;
	AnchorVelocity = FVector::ZeroVector;
	RopeDirection = Direction.GetSafeNormal();
	RopeEndLocation = Anchor;
	CurrentRopeLength = FMath::Min(8.0f, FMath::Max(0.0f, MaxRopeLength));
	SimulationAccumulator = 0.0f;
	bTipAnchored = false;
	bSimulatingRope = true;
	bIsExtending = CurrentRopeLength < MaxRopeLength;

	// Verlet stores velocity as the difference between current and previous position.
	// This gives the free end an actual launch impulse instead of teleporting it outward.
	const FVector LaunchVelocity = RopeDirection * FMath::Max(0.0f, ThrowSpeed) + AnchorVelocity;
	PreviousParticlePositions.Last() = Anchor - LaunchVelocity * SimulationStep;

	UpdateRopeVisual();
}

void AWeapon_Rope::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bSimulatingRope || ParticlePositions.Num() < 2 || !GetWorld())
	{
		LastAnchorLocation = GetActorLocation();
		return;
	}

	const FVector CurrentAnchor = GetActorLocation();
	if (DeltaSeconds > SMALL_NUMBER)
	{
		AnchorVelocity = (CurrentAnchor - LastAnchorLocation) / DeltaSeconds;
	}
	else
	{
		AnchorVelocity = FVector::ZeroVector;
	}
	LastAnchorLocation = CurrentAnchor;
	ParticlePositions[0] = CurrentAnchor;
	PreviousParticlePositions[0] = CurrentAnchor;

	// Fixed-step simulation keeps the rope stable across variable frame rates.
	SimulationAccumulator += FMath::Min(DeltaSeconds, 0.1f);
	while (SimulationAccumulator >= SimulationStep)
	{
		SimulateRopeStep(SimulationStep);
		SimulationAccumulator -= SimulationStep;
	}

	RopeStartLocation = CurrentAnchor;
	RopeEndLocation = ParticlePositions.Last();
	UpdateRopeVisual();
}

void AWeapon_Rope::SimulateRopeStep(float StepSeconds)
{
	if (bIsExtending)
	{
		CurrentRopeLength = FMath::Min(
			FMath::Max(0.0f, MaxRopeLength),
			CurrentRopeLength + FMath::Max(0.0f, ExtensionSpeed) * StepSeconds);

		if (CurrentRopeLength >= MaxRopeLength)
		{
			bIsExtending = false;
		}
	}

	const FVector Gravity(0.0f, 0.0f, GetWorld()->GetGravityZ() * GravityScale);
	const float Damping = FMath::Clamp(1.0f - VelocityDamping, 0.0f, 1.0f);

	for (int32 Index = 1; Index < ParticlePositions.Num(); ++Index)
	{
		if (bTipAnchored && Index == ParticlePositions.Num() - 1)
		{
			continue;
		}

		const FVector Current = ParticlePositions[Index];
		const FVector Velocity = (Current - PreviousParticlePositions[Index]) * Damping;
		const FVector Next = Current + Velocity + Gravity * (StepSeconds * StepSeconds);
		PreviousParticlePositions[Index] = Current;
		ParticlePositions[Index] = Next;
	}

	SolveRopeConstraints();
	ResolveParticleCollisions();
	SolveRopeConstraints();

	ParticlePositions[0] = GetActorLocation();
	if (bTipAnchored && ParticlePositions.Num() > 1)
	{
		PreviousParticlePositions.Last() = ParticlePositions.Last();
	}
}

void AWeapon_Rope::SolveRopeConstraints()
{
	const int32 ParticleCount = ParticlePositions.Num();
	if (ParticleCount < 2)
	{
		return;
	}

	const float SegmentRestLength = CurrentRopeLength / static_cast<float>(ParticleCount - 1);
	const int32 Iterations = FMath::Clamp(ConstraintIterations, 1, 20);

	for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
	{
		ParticlePositions[0] = GetActorLocation();

		for (int32 Index = 0; Index < ParticleCount - 1; ++Index)
		{
			FVector& PointA = ParticlePositions[Index];
			FVector& PointB = ParticlePositions[Index + 1];
			const FVector Delta = PointB - PointA;
			const float Distance = Delta.Size();
			if (Distance <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector Correction = Delta * ((Distance - SegmentRestLength) / Distance);
			const bool bAIsFixed = Index == 0;
			const bool bBIsFixed = bTipAnchored && Index + 1 == ParticleCount - 1;

			if (bAIsFixed && !bBIsFixed)
			{
				PointB -= Correction;
			}
			else if (!bAIsFixed && bBIsFixed)
			{
				PointA += Correction;
			}
			else if (!bAIsFixed && !bBIsFixed)
			{
				PointA += Correction * 0.5f;
				PointB -= Correction * 0.5f;
			}
		}
	}
}

void AWeapon_Rope::ResolveParticleCollisions()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RopeParticleCollision), true, this);
	if (OwningCharacter.IsValid())
	{
		QueryParams.AddIgnoredActor(OwningCharacter.Get());
	}

	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(FMath::Max(1.0f, RopeCollisionRadius));
	for (int32 Index = 1; Index < ParticlePositions.Num(); ++Index)
	{
		if (bTipAnchored && Index == ParticlePositions.Num() - 1)
		{
			continue;
		}

		const FVector Previous = PreviousParticlePositions[Index];
		const FVector Current = ParticlePositions[Index];
		FHitResult Hit;
		if (World->SweepSingleByChannel(
			Hit, Previous, Current, FQuat::Identity, RopeTraceChannel, CollisionShape, QueryParams)
			&& Hit.bBlockingHit)
		{
			ParticlePositions[Index] = Hit.Location;
			PreviousParticlePositions[Index] = Hit.Location;

			if (Index == ParticlePositions.Num() - 1)
			{
				bTipAnchored = true;
				bIsExtending = false;
				UE_LOG(LogTemp, Verbose, TEXT("[AWeapon_Rope] Rope tip collided with %s."), *GetNameSafe(Hit.GetActor()));
			}
		}
	}
}

void AWeapon_Rope::ResetRopeVisual()
{
	bIsExtending = false;
	bSwingActive = false;
	bSimulatingRope = false;
	bTipAnchored = false;
	CurrentRopeLength = 0.0f;
	SimulationAccumulator = 0.0f;
	ParticlePositions.Empty();
	PreviousParticlePositions.Empty();
	RopeStartLocation = GetActorLocation();
	LastAnchorLocation = RopeStartLocation;
	AnchorVelocity = FVector::ZeroVector;
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
		RopeSpline->ClearSplinePoints(false);
		RopeSpline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
		RopeSpline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
		RopeSpline->UpdateSpline();
	}
}

void AWeapon_Rope::UpdateRopeVisual()
{
	if (!IsValid(RopeSpline) || !IsValid(RopeSegmentMesh) || ParticlePositions.Num() < 2)
	{
		for (USplineMeshComponent* Segment : RopeSegments)
		{
			if (IsValid(Segment)) Segment->SetVisibility(false);
		}
		return;
	}

	RopeSpline->ClearSplinePoints(false);
	for (const FVector& WorldPosition : ParticlePositions)
	{
		const FVector LocalPosition = GetActorTransform().InverseTransformPosition(WorldPosition);
		RopeSpline->AddSplinePoint(LocalPosition, ESplineCoordinateSpace::Local, false);
	}
	for (int32 Index = 0; Index < ParticlePositions.Num(); ++Index)
	{
		RopeSpline->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}
	RopeSpline->UpdateSpline();

	const float SplineLength = RopeSpline->GetSplineLength();
	const int32 ActiveSegmentCount = FMath::Min(ParticlePositions.Num() - 1, RopeSegments.Num());
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
