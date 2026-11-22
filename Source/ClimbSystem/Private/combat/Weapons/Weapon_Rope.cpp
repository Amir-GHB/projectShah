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
#include "Components/SkeletalMeshComponent.h"
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

void AWeapon_Rope::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRopeVisual();
	Super::EndPlay(EndPlayReason);
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
	FVector AimLocation;
	AEnemyCharacter* SelectedTarget = nullptr;
	if (!ResolveThrowDirection(Direction, AimLocation, SelectedTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Rope] Could not resolve a valid throw direction."));
		return;
	}

	// Lock the intended enemy before the rope leaves the hand. Catching the
	// target is now based on the deployed endpoint reaching this aim point,
	// not on a broad collision sweep discovering an enemy by accident.
	PullTarget = SelectedTarget;
	bSwingActive = false;
	InitializeRopeSimulation(Direction, AimLocation, true);
	UE_LOG(LogTemp, Verbose, TEXT("[AWeapon_Rope] Rope physically thrown. Max length: %.1f"), MaxRopeLength);
}

bool AWeapon_Rope::ResolveThrowDirection(
	FVector& OutDirection,
	FVector& OutAimLocation,
	AEnemyCharacter*& OutTarget) const
{
	ACharacter* Character = OwningCharacter.Get();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World)
	{
		return false;
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

	const FVector Anchor = GetActorLocation();
	const float SafeMaximum = FMath::Max(100.0f, MaxRopeLength - FMath::Max(0.0f, EndSlackLength));
	auto GetEnemyAimPoint = [](const AEnemyCharacter* Enemy)
	{
		if (IsValid(Enemy) && IsValid(Enemy->GetMesh()))
		{
			const FBox MeshBounds = Enemy->GetMesh()->Bounds.GetBox();
			if (MeshBounds.IsValid)
			{
				return FVector(MeshBounds.GetCenter().X, MeshBounds.GetCenter().Y,
					FMath::Lerp(MeshBounds.Min.Z, MeshBounds.Max.Z, 0.68f));
			}
		}
		if (IsValid(Enemy))
		{
			FVector BoundsOrigin;
			FVector BoundsExtent;
			Enemy->GetActorBounds(false, BoundsOrigin, BoundsExtent);
			return BoundsOrigin + FVector(0.0f, 0.0f, BoundsExtent.Z * 0.25f);
		}
		return FVector::ZeroVector;
	};
	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		return false;
	}

	OutTarget = nullptr;
	AActor* Candidate = GetTargetingActor();
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Candidate))
	{
		const UDamageHandlerComponent* Damage = Enemy->DamageHandlerComponent;
		const FVector CandidatePoint = GetEnemyAimPoint(Enemy);
		const float CandidateDistance = FVector::Dist(Anchor, CandidatePoint);

		FCollisionQueryParams TargetParams(SCENE_QUERY_STAT(RopePreThrowTarget), true, Character);
		TargetParams.AddIgnoredActor(this);
		FHitResult VisibilityHit;
		const bool bBlocked = World->LineTraceSingleByChannel(
			VisibilityHit, ViewLocation, CandidatePoint, ECC_Visibility, TargetParams);

		if ((!Damage || !Damage->bIsDead)
			&& !Enemy->bIsInDamageReaction
			&& CandidateDistance <= SafeMaximum
			&& (!bBlocked || VisibilityHit.GetActor() == Enemy))
		{
			OutTarget = Enemy;
			OutAimLocation = CandidatePoint;
		}
	}

	// If target lock did not provide a valid in-range enemy, use the same
	// camera-forward visibility trace used by the bow to resolve the aim point.
	if (!IsValid(OutTarget))
	{
		FCollisionQueryParams AimParams(SCENE_QUERY_STAT(RopeAimTrace), true, Character);
		AimParams.AddIgnoredActor(this);
		FHitResult AimHit;
		const FVector TraceEnd = ViewLocation + ViewDirection * 10000.0f;
		OutAimLocation = TraceEnd;
		const bool bViewHit = World->LineTraceSingleByChannel(
			AimHit, ViewLocation, TraceEnd, ECC_Visibility, AimParams) && AimHit.bBlockingHit;
		if (bViewHit)
		{
			OutAimLocation = AimHit.ImpactPoint;

			// A center-view hit on an enemy becomes the preselected rope target.
			// This mirrors the bow's camera trace while keeping the rope's target
			// locked before its release notify fires.
			if (AEnemyCharacter* TraceEnemy = Cast<AEnemyCharacter>(AimHit.GetActor()))
			{
				const UDamageHandlerComponent* Damage = TraceEnemy->DamageHandlerComponent;
				const FVector EnemyPoint = GetEnemyAimPoint(TraceEnemy);
				if ((!Damage || !Damage->bIsDead)
					&& !TraceEnemy->bIsInDamageReaction
					&& FVector::Dist(Anchor, EnemyPoint) <= SafeMaximum)
				{
					OutTarget = TraceEnemy;
					OutAimLocation = EnemyPoint;
				}
			}
		}

		const FVector ToAim = OutAimLocation - Anchor;
		// With no blocking point, reserve a little material length for the
		// upward loft. The curved path still consumes the full available rope.
		const float ArcReserve = FMath::Min(150.0f, FMath::Max(0.0f, EndSlackLength * 0.5f));
		const float FreeAimMaximum = bViewHit
			? SafeMaximum : FMath::Max(100.0f, SafeMaximum - ArcReserve);
		if (ToAim.SizeSquared() > FMath::Square(FreeAimMaximum))
		{
			OutAimLocation = Anchor + ToAim.GetSafeNormal() * FreeAimMaximum;
			OutTarget = nullptr;
		}
	}

	const FVector ToAim = OutAimLocation - Anchor;
	if (ToAim.IsNearlyZero())
	{
		return false;
	}

	// The rope follows a lofted path that still terminates at the selected
	// point. The spare rope length is used for the upward arc; at maximum
	// reach the arc naturally flattens rather than overshooting the target.
	const FVector Horizontal = FVector(ToAim.X, ToAim.Y, 0.0f);
	if (Horizontal.IsNearlyZero())
	{
		OutDirection = ToAim.GetSafeNormal();
	}
	else
	{
		OutDirection = (Horizontal.GetSafeNormal() + FVector::UpVector * 0.25f).GetSafeNormal();
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

void AWeapon_Rope::InitializeRopeSimulation(
	const FVector& Direction,
	const FVector& AimLocation,
	bool bHasAimLocation)
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
	ParticleLaunchVelocities.SetNum(ParticleCount);

	RopeStartLocation = Anchor;
	LastAnchorLocation = Anchor;
	ThrowAimLocation = bHasAimLocation ? AimLocation : Anchor + Direction.GetSafeNormal() * SafeMaxLength;
	bHasThrowAimLocation = bHasAimLocation;
	RopeEndLocation = Anchor;
	CurrentRopeLength = 0.0f;
	SimulationAccumulator = 0.0f;
	bSimulatingRope = true;
	bIsExtending = SafeMaxLength > KINDA_SMALL_NUMBER;

	const float SlackLength = FMath::Clamp(EndSlackLength, 0.0f, SafeMaxLength);
	const float MaximumPathLength = FMath::Max(0.0f, SafeMaxLength - SlackLength);
	const FVector ToAim = ThrowAimLocation - Anchor;
	const float ChordLength = FMath::Min(ToAim.Size(), MaximumPathLength);
	const FVector EndPoint = ToAim.IsNearlyZero()
		? Anchor + Direction.GetSafeNormal() * ChordLength
		: Anchor + ToAim.GetSafeNormal() * ChordLength;

	// Use a quadratic Bezier path. Its control point is lifted above the
	// midpoint, and the lift is limited by the spare rope length so the path
	// plus the loose end slack never exceeds MaxRopeLength.
	const float DesiredPathLength = FMath::Min(
		MaximumPathLength,
		ChordLength + FMath::Min(150.0f, ChordLength * 0.20f));
	const FVector MidPoint = (Anchor + EndPoint) * 0.5f;
	auto BuildCurve = [&](float Height, TArray<FVector>& OutPoints, TArray<float>& OutDistances)
	{
		OutPoints.Reset();
		OutDistances.Reset();
		const FVector Control = MidPoint + FVector::UpVector * Height;
		constexpr int32 CurveSamples = 64;
		float TotalLength = 0.0f;
		for (int32 Sample = 0; Sample <= CurveSamples; ++Sample)
		{
			const float T = static_cast<float>(Sample) / static_cast<float>(CurveSamples);
			const float OneMinusT = 1.0f - T;
			const FVector Point = Anchor * (OneMinusT * OneMinusT)
				+ Control * (2.0f * OneMinusT * T)
				+ EndPoint * (T * T);
			if (!OutPoints.IsEmpty())
			{
				TotalLength += FVector::Distance(OutPoints.Last(), Point);
			}
			OutPoints.Add(Point);
			OutDistances.Add(TotalLength);
		}
		return TotalLength;
	};

	TArray<FVector> CurvePoints;
	TArray<float> CurveDistances;
	float LowHeight = 0.0f;
	float HighHeight = FMath::Max(1.0f, SafeMaxLength);
	for (int32 Iteration = 0; Iteration < 20; ++Iteration)
	{
		const float TestHeight = (LowHeight + HighHeight) * 0.5f;
		TArray<FVector> TestPoints;
		TArray<float> TestDistances;
		const float TestLength = BuildCurve(TestHeight, TestPoints, TestDistances);
		if (TestLength > DesiredPathLength)
		{
			HighHeight = TestHeight;
		}
		else
		{
			LowHeight = TestHeight;
		}
	}
	TargetThrowDistance = BuildCurve(LowHeight, CurvePoints, CurveDistances);
	TargetThrowDistance = FMath::Min(TargetThrowDistance, MaximumPathLength);
	const float ActualSlackLength = FMath::Max(0.0f, SafeMaxLength - TargetThrowDistance);
	const float SlackRadius = ActualSlackLength / PI;
	const FVector EndTangent = (CurvePoints.Last() - CurvePoints[CurvePoints.Num() - 2]).GetSafeNormal();

	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		const float MaterialDistance = SegmentRestLength * Index;
		FVector Position = Anchor;
		FVector Tangent = Direction.GetSafeNormal();

		if (MaterialDistance <= TargetThrowDistance && CurvePoints.Num() > 1)
		{
			int32 SampleIndex = 1;
			while (SampleIndex < CurveDistances.Num() && CurveDistances[SampleIndex] < MaterialDistance)
			{
				++SampleIndex;
			}
			SampleIndex = FMath::Clamp(SampleIndex, 1, CurvePoints.Num() - 1);
			const float SegmentDistance = CurveDistances[SampleIndex] - CurveDistances[SampleIndex - 1];
			const float Alpha = SegmentDistance > KINDA_SMALL_NUMBER
				? FMath::Clamp((MaterialDistance - CurveDistances[SampleIndex - 1]) / SegmentDistance, 0.0f, 1.0f)
				: 0.0f;
			Position = FMath::Lerp(CurvePoints[SampleIndex - 1], CurvePoints[SampleIndex], Alpha);
			Tangent = (CurvePoints[SampleIndex] - CurvePoints[SampleIndex - 1]).GetSafeNormal();
		}
		else if (ActualSlackLength > KINDA_SMALL_NUMBER)
		{
			const float SlackDistance = FMath::Clamp(MaterialDistance - TargetThrowDistance, 0.0f, ActualSlackLength);
			const float Angle = SlackRadius > KINDA_SMALL_NUMBER ? SlackDistance / SlackRadius : 0.0f;
			Position = EndPoint
				+ EndTangent * (SlackRadius * FMath::Sin(Angle))
				+ FVector::DownVector * (SlackRadius * (1.0f - FMath::Cos(Angle)));
			Tangent = (EndTangent * FMath::Cos(Angle) + FVector::DownVector * FMath::Sin(Angle)).GetSafeNormal();
		}
		else
		{
			Position = EndPoint;
			Tangent = EndTangent;
		}

		ParticleDeploymentPositions[Index] = Position;
		ParticleLaunchVelocities[Index] = Tangent * FMath::Max(0.0f, ThrowSpeed);
	}

	RopeDirection = CurvePoints.Num() > 1
		? (CurvePoints[1] - CurvePoints[0]).GetSafeNormal()
		: Direction.GetSafeNormal();
	RopeEndLocation = ParticleDeploymentPositions.Last();
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
			const FVector LaunchVelocity = (ParticleLaunchVelocities.IsValidIndex(Index)
				? ParticleLaunchVelocities[Index] : RopeDirection * FMath::Max(0.0f, ThrowSpeed)) + AnchorVelocity;
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

	// The intended target was selected before release. Catch it when the
	// deployed rope path reaches its stored aim point; no pawn collision sweep
	// is used to discover or choose the target.
	const float SegmentRestLength = FMath::Max(
		1.0f, MaxRopeLength / FMath::Max(1, ParticlePositions.Num() - 1));
	const float CatchRadius = FMath::Max(25.0f, RopeCollisionRadius * 4.0f)
		+ SegmentRestLength * 0.75f;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (int32 Index = 1; Index <= LastDeployedIndex; ++Index)
	{
		ClosestDistanceSquared = FMath::Min(
			ClosestDistanceSquared,
			FVector::DistSquared(ParticlePositions[Index], ThrowAimLocation));
	}

	if (ClosestDistanceSquared <= FMath::Square(CatchRadius))
	{
		BeginTargetPull(Target);
		return bIsPullingTarget;
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
	ThrowAimLocation = FVector::ZeroVector;
	bHasThrowAimLocation = false;
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
	ParticleLaunchVelocities.Empty();
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
