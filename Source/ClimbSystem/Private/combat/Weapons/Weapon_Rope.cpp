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
#include "Components/CapsuleComponent.h"
#include "combat/EnemyCharacter.h"
#include "combat/DamageHandlerComponent.h"
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

	PullTarget = Cast<AEnemyCharacter>(GetTargetingActor());

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

	AActor* Target = GetTargetingActor();

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

AActor* AWeapon_Rope::GetTargetingActor() const
{
	const ACharacter* Character = OwningCharacter.Get();
	const UCombatTargetingComponent* Targeting = Character
		? Character->FindComponentByClass<UCombatTargetingComponent>() : nullptr;
	if (!Targeting)
	{
		return nullptr;
	}
	AActor* Target = Targeting->GetCurrentTarget();
	return IsValid(Target) ? Target : Targeting->GetManualTarget();
}

void AWeapon_Rope::InitializeRopeSimulation(const FVector& Direction)
{
	const int32 SegmentCount = FMath::Clamp(RopeSegmentCount, 1, MaxRopeSegments);
	const int32 ParticleCount = SegmentCount + 1;
	const FVector Anchor = GetActorLocation();
	const float SafeMaxLength = FMath::Max(0.0f, MaxRopeLength);
	const float SegmentRestLength = SafeMaxLength / static_cast<float>(SegmentCount);

	ParticlePositions.Init(Anchor, ParticleCount);
	PreviousParticlePositions.Init(Anchor, ParticleCount);
	bParticleDeployed.Init(false, ParticleCount);
	ParticleDeploymentPositions.SetNum(ParticleCount);

	RopeStartLocation = Anchor;
	LastAnchorLocation = Anchor;
	RopeDirection = Direction.GetSafeNormal();
	RopeEndLocation = Anchor;
	CurrentRopeLength = 0.0f;
	TargetThrowDistance = FMath::Clamp(SafeMaxLength - EndSlackLength, 0.0f, SafeMaxLength);
	SimulationAccumulator = 0.0f;
	bSimulatingRope = true;
	bIsExtending = SafeMaxLength > KINDA_SMALL_NUMBER;

	// The deployed portion travels straight. The remaining rope is laid into a
	// broad U-shaped tail at the end, so unused length gathers at the free end
	// instead of being distributed uniformly along the entire rope.
	const float SlackLength = SafeMaxLength - TargetThrowDistance;
	const float SlackRadius = SlackLength / PI;
	const FVector Down = FVector::DownVector;
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		const float MaterialDistance = SegmentRestLength * Index;
		if (MaterialDistance <= TargetThrowDistance || SlackLength <= KINDA_SMALL_NUMBER)
		{
			ParticleDeploymentPositions[Index] = Anchor + RopeDirection * FMath::Min(MaterialDistance, TargetThrowDistance);
		}
		else
		{
			const float SlackArcDistance = FMath::Clamp(MaterialDistance - TargetThrowDistance, 0.0f, SlackLength);
			const float Angle = SlackRadius > KINDA_SMALL_NUMBER ? SlackArcDistance / SlackRadius : 0.0f;
			const FVector SlackOffset = RopeDirection * (SlackRadius * FMath::Sin(Angle))
				+ Down * (SlackRadius * (1.0f - FMath::Cos(Angle)));
			ParticleDeploymentPositions[Index] = Anchor + RopeDirection * TargetThrowDistance + SlackOffset;
		}
	}

	bParticleDeployed[0] = true;
	ParticlePositions[0] = Anchor;
	PreviousParticlePositions[0] = Anchor;
	UpdateRopeVisual();
}

void AWeapon_Rope::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsRetracting)
	{
		UpdateRopeRetraction(DeltaSeconds);
		return;
	}

	if (bIsPullingTarget)
	{
		UpdateTargetPull(DeltaSeconds);
		return;
	}

	if (!bSimulatingRope || ParticlePositions.Num() < 2 || !GetWorld())
	{
		const FVector IdleAnchor = GetActorLocation();
		AnchorVelocity = DeltaSeconds > SMALL_NUMBER
			? (IdleAnchor - LastAnchorLocation) / DeltaSeconds
			: FVector::ZeroVector;
		LastAnchorLocation = IdleAnchor;
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

	if (TryBeginTargetPull())
	{
		UpdateTargetPull(DeltaSeconds);
		return;
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

		const float SegmentRestLength = FMath::Max(1.0f, MaxRopeLength / FMath::Max(1, ParticlePositions.Num() - 1));
		for (int32 Index = 1; Index < ParticlePositions.Num(); ++Index)
		{
			if (bParticleDeployed[Index])
			{
				continue;
			}

			const float MaterialDistance = SegmentRestLength * Index;
			if (MaterialDistance > CurrentRopeLength + KINDA_SMALL_NUMBER)
			{
				break;
			}

			const FVector SpawnPosition = ParticleDeploymentPositions[Index];
			ParticlePositions[Index] = SpawnPosition;
			const FVector LaunchVelocity = RopeDirection * FMath::Max(0.0f, ThrowSpeed) + AnchorVelocity;
			PreviousParticlePositions[Index] = SpawnPosition - LaunchVelocity * StepSeconds;
			bParticleDeployed[Index] = true;
		}

		if (CurrentRopeLength >= MaxRopeLength)
		{
			bIsExtending = false;
		}
	}

	const FVector Gravity(0.0f, 0.0f, GetWorld()->GetGravityZ() * GravityScale);
	const float Damping = FMath::Clamp(1.0f - VelocityDamping, 0.0f, 1.0f);

	for (int32 Index = 1; Index < ParticlePositions.Num(); ++Index)
	{
		if (!bParticleDeployed[Index])
		{
			continue;
		}

		const FVector Current = ParticlePositions[Index];
		const FVector Velocity = (Current - PreviousParticlePositions[Index]) * Damping;
		PreviousParticlePositions[Index] = Current;
		ParticlePositions[Index] = Current + Velocity + Gravity * (StepSeconds * StepSeconds);
	}

	SolveRopeConstraints();
	ResolveParticleCollisions();
	SolveRopeConstraints();

	ParticlePositions[0] = GetActorLocation();
}

void AWeapon_Rope::SolveRopeConstraints()
{
	const int32 ParticleCount = ParticlePositions.Num();
	if (ParticleCount < 2)
	{
		return;
	}

	// Rope links have a fixed maximum length. They are allowed to compress,
	// which lets excess rope bunch up at the free end rather than being forced
	// into evenly spaced particles along the whole line.
	const float SegmentRestLength = FMath::Max(1.0f, MaxRopeLength / static_cast<float>(ParticleCount - 1));
	const int32 Iterations = FMath::Clamp(ConstraintIterations, 1, 20);

	for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
	{
		ParticlePositions[0] = GetActorLocation();

		for (int32 Index = 0; Index < ParticleCount - 1; ++Index)
		{
			if (!bParticleDeployed.IsValidIndex(Index + 1) || !bParticleDeployed[Index + 1])
			{
				break;
			}

			FVector& PointA = ParticlePositions[Index];
			FVector& PointB = ParticlePositions[Index + 1];
			const FVector Delta = PointB - PointA;
			const float Distance = Delta.Size();
			if (Distance <= SegmentRestLength || Distance <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector Correction = Delta * ((Distance - SegmentRestLength) / Distance);
			if (Index == 0)
			{
				PointB -= Correction;
			}
			else
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

	const float SafeRadius = FMath::Max(1.0f, RopeCollisionRadius);
	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(SafeRadius);
	int32 LastDeployedIndex = 0;
	while (bParticleDeployed.IsValidIndex(LastDeployedIndex + 1) && bParticleDeployed[LastDeployedIndex + 1])
	{
		++LastDeployedIndex;
	}
	for (int32 Index = 1; Index < ParticlePositions.Num(); ++Index)
	{
		if (!bParticleDeployed.IsValidIndex(Index) || !bParticleDeployed[Index])
		{
			break;
		}

		const FVector Previous = PreviousParticlePositions[Index];
		const FVector Current = ParticlePositions[Index];
		FHitResult Hit;
		if (World->SweepSingleByChannel(
			Hit, Previous, Current, FQuat::Identity, RopeTraceChannel, CollisionShape, QueryParams)
			&& Hit.bBlockingHit)
		{
			// Resolve against the surface without pinning the particle there.
			// Keep tangential momentum so the rope can slide and fall away.
			const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
			const FVector SurfacePosition = Hit.Location + Normal * 0.5f;
			FVector Velocity = Current - Previous;
			const float IntoSurface = FVector::DotProduct(Velocity, Normal);
			if (IntoSurface < 0.0f)
			{
				Velocity -= Normal * IntoSurface;
			}
			Velocity *= 0.85f;

			ParticlePositions[Index] = SurfacePosition;
			PreviousParticlePositions[Index] = SurfacePosition - Velocity;

			// A collision at the currently leading end stops further rope payout.
			// The endpoint itself remains free and is never attached to the hit actor/world.
			if (bIsExtending && Index == LastDeployedIndex)
			{
				bIsExtending = false;
				UE_LOG(LogTemp, Verbose, TEXT("[AWeapon_Rope] Rope payout stopped by obstruction: %s"), *GetNameSafe(Hit.GetActor()));
			}
		}
	}
}

bool AWeapon_Rope::TryBeginTargetPull()
{
	AEnemyCharacter* Target = PullTarget.Get();
	if (!IsValid(Target) || !GetWorld() || ParticlePositions.Num() < 2)
	{
		return false;
	}

	const UDamageHandlerComponent* Damage = Target->DamageHandlerComponent;
	if ((Damage && Damage->bIsDead) || Target->bIsInDamageReaction)
	{
		PullTarget.Reset();
		return false;
	}

	int32 LastDeployedIndex = INDEX_NONE;
	for (int32 Index = 0; Index < bParticleDeployed.Num(); ++Index)
	{
		if (bParticleDeployed[Index]) LastDeployedIndex = Index;
		else break;
	}
	if (LastDeployedIndex < 1) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeTargetCatch), true, this);
	if (AActor* OwnerActor = GetOwner()) Params.AddIgnoredActor(OwnerActor);
	Params.AddIgnoredActor(this);
	const float CatchRadius = FMath::Max(20.0f, RopeCollisionRadius * 3.0f);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(
		Hits, ParticlePositions[0], ParticlePositions[LastDeployedIndex], FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(CatchRadius), Params);

	for (const FHitResult& Hit : Hits)
	{
		if (Hit.GetActor() == Target)
		{
			BeginTargetPull(Target);
			return bIsPullingTarget;
		}
	}
	return false;
}

void AWeapon_Rope::BeginTargetPull(AEnemyCharacter* Target)
{
	if (!IsValid(Target) || bIsPullingTarget) return;

	const UDamageHandlerComponent* Damage = Target->DamageHandlerComponent;
	if ((Damage && Damage->bIsDead) || Target->bIsInDamageReaction) return;
	const float OwnerRadius = OwningCharacter.IsValid() && OwningCharacter->GetCapsuleComponent()
		? OwningCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;
	const float TargetRadius = Target->GetCapsuleComponent()
		? Target->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;
	const float StopDistance = OwnerRadius + TargetRadius + FMath::Max(0.0f, PullStopPadding);
	if (!Target->BeginRopePull(this, EnemyPullSpeed, StopDistance))
	{
		PullTarget.Reset();
		return;
	}

	PullTarget = Target;
	bIsPullingTarget = true;
	bIsExtending = false;
	for (int32 Index = 0; Index < bParticleDeployed.Num(); ++Index)
	{
		bParticleDeployed[Index] = true;
	}

	UE_LOG(LogTemp, Log, TEXT("[AWeapon_Rope] Caught enemy %s. Starting root pull."), *GetNameSafe(Target));
}

void AWeapon_Rope::UpdateTargetPull(float DeltaSeconds)
{
	AEnemyCharacter* Target = PullTarget.Get();
	if (!IsValid(Target) || (Target->DamageHandlerComponent && Target->DamageHandlerComponent->bIsDead))
	{
		ResetRopeVisual();
		return;
	}

	// The enemy ends its temporary pull state when it reaches the player or
	// encounters an obstruction. Keep the rope visible briefly while it reels
	// back into the weapon instead of deleting it immediately.
	if (!Target->bIsBeingRopePulled)
	{
		BeginRopeRetraction();
		return;
	}

	const FVector Anchor = GetActorLocation();
	if (ParticlePositions.Num() > 0)
	{
		ParticlePositions[0] = Anchor;
		PreviousParticlePositions[0] = Anchor;
	}

	const FVector TargetPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f,
		Target->GetCapsuleComponent() ? Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.35f : 35.0f);
	const int32 LastIndex = ParticlePositions.Num() - 1;
	for (int32 Index = 1; Index <= LastIndex; ++Index)
	{
		if (!bParticleDeployed.IsValidIndex(Index) || !bParticleDeployed[Index]) break;
		const float Alpha = static_cast<float>(Index) / static_cast<float>(LastIndex);
		const FVector Desired = FMath::Lerp(Anchor, TargetPoint, Alpha);
		ParticlePositions[Index] = FMath::VInterpTo(
			ParticlePositions[Index], Desired, DeltaSeconds, FMath::Max(0.1f, RopeContractionSpeed));
		PreviousParticlePositions[Index] = ParticlePositions[Index];
	}
	if (bParticleDeployed.IsValidIndex(LastIndex) && bParticleDeployed[LastIndex])
	{
		ParticlePositions[LastIndex] = TargetPoint;
		PreviousParticlePositions[LastIndex] = TargetPoint;
	}

	RopeStartLocation = Anchor;
	RopeEndLocation = TargetPoint;
	UpdateRopeVisual();
}

void AWeapon_Rope::BeginRopeRetraction()
{
	if (!bIsPullingTarget)
	{
		return;
	}

	AEnemyCharacter* Target = PullTarget.Get();
	if (IsValid(Target) && Target->bIsBeingRopePulled)
	{
		Target->EndRopePull();
	}

	const FVector Anchor = GetActorLocation();
	const FVector EndLocation = ParticlePositions.Num() > 0
		? ParticlePositions.Last() : RopeEndLocation;
	const FVector Delta = EndLocation - Anchor;
	RetractionLength = Delta.Size();
	RetractionDirection = Delta.GetSafeNormal();
	bIsPullingTarget = false;
	bIsRetracting = RetractionLength > KINDA_SMALL_NUMBER;
	PullTarget.Reset();
	bIsExtending = false;

	if (!bIsRetracting)
	{
		ResetRopeVisual();
	}
}

void AWeapon_Rope::UpdateRopeRetraction(float DeltaSeconds)
{
	RetractionLength = FMath::Max(0.0f,
		RetractionLength - FMath::Max(0.0f, RopeRetractSpeed) * DeltaSeconds);

	const FVector Anchor = GetActorLocation();
	const FVector EndLocation = Anchor + RetractionDirection * RetractionLength;
	const int32 LastIndex = ParticlePositions.Num() - 1;
	for (int32 Index = 0; Index <= LastIndex; ++Index)
	{
		const float Alpha = LastIndex > 0
			? static_cast<float>(Index) / static_cast<float>(LastIndex) : 0.0f;
		const FVector Position = FMath::Lerp(Anchor, EndLocation, Alpha);
		ParticlePositions[Index] = Position;
		PreviousParticlePositions[Index] = Position;
		if (bParticleDeployed.IsValidIndex(Index))
		{
			bParticleDeployed[Index] = true;
		}
	}

	CurrentRopeLength = RetractionLength;
	RopeStartLocation = Anchor;
	RopeEndLocation = EndLocation;
	UpdateRopeVisual();

	if (RetractionLength <= KINDA_SMALL_NUMBER)
	{
		ResetRopeVisual();
	}
}

void AWeapon_Rope::ResetRopeVisual()
{
	if (AEnemyCharacter* Target = PullTarget.Get())
	{
		Target->EndRopePull();
	}
	PullTarget.Reset();
	bIsPullingTarget = false;
	bIsRetracting = false;
	RetractionLength = 0.0f;
	bIsExtending = false;
	bSwingActive = false;
	bSimulatingRope = false;
	CurrentRopeLength = 0.0f;
	SimulationAccumulator = 0.0f;
	ParticlePositions.Empty();
	PreviousParticlePositions.Empty();
	bParticleDeployed.Empty();
	ParticleDeploymentPositions.Empty();
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
	int32 ActiveParticleCount = 0;
	while (bParticleDeployed.IsValidIndex(ActiveParticleCount) && bParticleDeployed[ActiveParticleCount])
	{
		++ActiveParticleCount;
	}

	if (!IsValid(RopeSpline) || !IsValid(RopeSegmentMesh) || ActiveParticleCount < 2)
	{
		for (USplineMeshComponent* Segment : RopeSegments)
		{
			if (IsValid(Segment)) Segment->SetVisibility(false);
		}
		return;
	}

	RopeSpline->ClearSplinePoints(false);
	for (int32 Index = 0; Index < ActiveParticleCount; ++Index)
	{
		const FVector LocalPosition = GetActorTransform().InverseTransformPosition(ParticlePositions[Index]);
		RopeSpline->AddSplinePoint(LocalPosition, ESplineCoordinateSpace::Local, false);
	}
	for (int32 Index = 0; Index < ActiveParticleCount; ++Index)
	{
		RopeSpline->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}
	RopeSpline->UpdateSpline();

	const float SplineLength = RopeSpline->GetSplineLength();
	const int32 ActiveSegmentCount = FMath::Min(ActiveParticleCount - 1, RopeSegments.Num());
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
