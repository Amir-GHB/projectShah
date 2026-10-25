// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/WeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "combat/DamageHandlerComponent.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false; // Tracing updates are driven by animation notifies

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponMesh->SetGenerateOverlapEvents(false);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	OwningCharacter = Cast<ACharacter>(GetOwner());
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelTrace();
	Super::EndPlay(EndPlayReason);
}

void AWeaponBase::AttachToHand(ACharacter* TargetCharacter)
{
	if (!TargetCharacter || !TargetCharacter->GetMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeaponBase::AttachToHand] TargetCharacter or Mesh is null!"));
		return;
	}

	OwningCharacter = TargetCharacter;
	SetOwner(TargetCharacter);

	AttachToComponent(
		TargetCharacter->GetMesh(),
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		WeaponConfig.HandSocketName
	);

	SetWeaponHidden(false);
}

void AWeaponBase::AttachToHolster(ACharacter* TargetCharacter)
{
	if (!TargetCharacter || !TargetCharacter->GetMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeaponBase::AttachToHolster] TargetCharacter or Mesh is null!"));
		return;
	}

	OwningCharacter = TargetCharacter;
	SetOwner(TargetCharacter);

	if (bNoHolster)
	{
		// A weapon configured without a holster should not remain floating in the world.
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetWeaponHidden(true);
		return;
	}

	AttachToComponent(
		TargetCharacter->GetMesh(),
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		WeaponConfig.HolsterSocketName
	);

	SetWeaponHidden(false);
}

void AWeaponBase::SetWeaponHidden(bool Hidden)
{
	SetActorHiddenInGame(Hidden);
}

void AWeaponBase::SampleSocketLocations(TMap<FName, FVector>& OutLocations) const
{
	OutLocations.Empty(ValidatedTraceSockets.Num());

	if (!WeaponMesh) return;

	for (const FName& SocketName : ValidatedTraceSockets)
	{
		const FVector Loc = WeaponMesh->GetSocketLocation(SocketName);
		if (!Loc.ContainsNaN())
		{
			OutLocations.Add(SocketName, Loc);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AWeaponBase::SampleSocketLocations] Socket '%s' returned NaN location!"), *SocketName.ToString());
		}
	}
}

void AWeaponBase::BeginTrace()
{
	if (!WeaponMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeaponBase::BeginTrace] Cannot begin trace: WeaponMesh is null on %s"), *GetName());
		bIsTracing = false;
		return;
	}

	// Validate sockets on the mesh before enabling trace
	ValidatedTraceSockets.Empty();
	for (const FName& SocketName : WeaponConfig.TraceSocketNames)
	{
		if (WeaponMesh->DoesSocketExist(SocketName))
		{
			ValidatedTraceSockets.Add(SocketName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AWeaponBase::BeginTrace] Trace socket '%s' does not exist on mesh '%s'!"),
				*SocketName.ToString(), *GetNameSafe(WeaponMesh->GetStaticMesh()));
		}
	}

	if (ValidatedTraceSockets.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[AWeaponBase::BeginTrace] No valid trace sockets found for %s! Aborting trace."), *GetName());
		bIsTracing = false;
		return;
	}

	bIsTracing = true;
	DamagedActorsThisSwing.Empty();
	PreviousSocketLocations.Empty();
	SampleSocketLocations(PreviousSocketLocations);
}

void AWeaponBase::TickTrace()
{
	if (!bIsTracing || ValidatedTraceSockets.IsEmpty()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	TMap<FName, FVector> CurrentSocketLocations;
	SampleSocketLocations(CurrentSocketLocations);

	if (CurrentSocketLocations.IsEmpty() || PreviousSocketLocations.IsEmpty())
	{
		PreviousSocketLocations = CurrentSocketLocations;
		return;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	if (OwningCharacter.IsValid())
	{
		ActorsToIgnore.Add(OwningCharacter.Get());
	}

	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(WeaponConfig.TraceRadius);
	FCollisionQueryParams QueryParams(TEXT("MeleeWeaponTrace"), false, this);
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	// Calculate maximum displacement across all sockets between previous tick and current tick
	float MaxDisplacement = 0.0f;
	for (const auto& Pair : CurrentSocketLocations)
	{
		if (const FVector* PrevPosPtr = PreviousSocketLocations.Find(Pair.Key))
		{
			MaxDisplacement = FMath::Max(MaxDisplacement, FVector::Dist(*PrevPosPtr, Pair.Value));
		}
	}

	// Performance-capped substep calculation for large frame deltas or fast rotations
	const int32 NumSubsteps = FMath::Clamp(
		FMath::CeilToInt(MaxDisplacement / FMath::Max(1.0f, WeaponConfig.TraceRadius * 2.0f)),
		1,
		FMath::Max(1, WeaponConfig.MaxTraceSubsteps)
	);

	TArray<FHitResult> AllHitResults;

	// Loop through substeps
	for (int32 Substep = 1; Substep <= NumSubsteps; ++Substep)
	{
		const float AlphaPrev = float(Substep - 1) / float(NumSubsteps);
		const float AlphaCurr = float(Substep) / float(NumSubsteps);

		// Sample interpolated socket positions for this substep
		TMap<FName, FVector> SubstepPrevLocations;
		TMap<FName, FVector> SubstepCurrLocations;

		for (const FName& SName : ValidatedTraceSockets)
		{
			const FVector* P0 = PreviousSocketLocations.Find(SName);
			const FVector* P1 = CurrentSocketLocations.Find(SName);
			if (P0 && P1)
			{
				SubstepPrevLocations.Add(SName, FMath::Lerp(*P0, *P1, AlphaPrev));
				SubstepCurrLocations.Add(SName, FMath::Lerp(*P0, *P1, AlphaCurr));
			}
		}

		// 1. Continuous Sweeps along each socket's motion path in this substep (supports 1 or more sockets)
		for (const FName& SName : ValidatedTraceSockets)
		{
			const FVector* PPrev = SubstepPrevLocations.Find(SName);
			const FVector* PCurr = SubstepCurrLocations.Find(SName);
			if (PPrev && PCurr)
			{
				TArray<FHitResult> SocketHits;
				World->SweepMultiByChannel(SocketHits, *PPrev, *PCurr, FQuat::Identity, WeaponConfig.TraceCollisionChannel, SphereShape, QueryParams);
				AllHitResults.Append(SocketHits);

				if (WeaponConfig.bDrawDebugTraces)
				{
					DrawDebugCapsule(
						World,
						(*PPrev + *PCurr) * 0.5f,
						(*PPrev - *PCurr).Size() * 0.5f + WeaponConfig.TraceRadius,
						WeaponConfig.TraceRadius,
						FRotationMatrix::MakeFromZ(*PCurr - *PPrev).ToQuat(),
						FColor::Green,
						false,
						0.4f
					);
				}
			}
		}

		// 2. Segment & Cross-Diagonal Sweeps along blade geometry (for 2 or more sockets)
		if (ValidatedTraceSockets.Num() >= 2)
		{
			for (int32 i = 0; i < ValidatedTraceSockets.Num() - 1; ++i)
			{
				const FName& S1 = ValidatedTraceSockets[i];
				const FName& S2 = ValidatedTraceSockets[i + 1];

				const FVector* P1Curr = SubstepCurrLocations.Find(S1);
				const FVector* P2Curr = SubstepCurrLocations.Find(S2);
				const FVector* P1Prev = SubstepPrevLocations.Find(S1);
				const FVector* P2Prev = SubstepPrevLocations.Find(S2);

				if (P1Curr && P2Curr)
				{
					TArray<FHitResult> SegmentHits;
					World->SweepMultiByChannel(SegmentHits, *P1Curr, *P2Curr, FQuat::Identity, WeaponConfig.TraceCollisionChannel, SphereShape, QueryParams);
					AllHitResults.Append(SegmentHits);

					if (P1Prev && P2Prev)
					{
						TArray<FHitResult> CrossHits1;
						World->SweepMultiByChannel(CrossHits1, *P1Prev, *P2Curr, FQuat::Identity, WeaponConfig.TraceCollisionChannel, SphereShape, QueryParams);
						AllHitResults.Append(CrossHits1);

						TArray<FHitResult> CrossHits2;
						World->SweepMultiByChannel(CrossHits2, *P2Prev, *P1Curr, FQuat::Identity, WeaponConfig.TraceCollisionChannel, SphereShape, QueryParams);
						AllHitResults.Append(CrossHits2);
					}
				}
			}
		}
	}

	// 3. Deduplicate and validate hits
	for (const FHitResult& Hit : AllHitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (CanDamageActor(HitActor) && !DamagedActorsThisSwing.Contains(HitActor))
		{
			DamagedActorsThisSwing.Add(HitActor);
			ProcessHit(Hit);
		}
	}

	PreviousSocketLocations = CurrentSocketLocations;
}

void AWeaponBase::EndTrace()
{
	bIsTracing = false;
	DamagedActorsThisSwing.Empty();
	PreviousSocketLocations.Empty();
	ValidatedTraceSockets.Empty();
}

void AWeaponBase::CancelTrace()
{
	EndTrace();
}

bool AWeaponBase::CanDamageActor(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor) || TargetActor->IsPendingKillPending())
	{
		return false;
	}

	if (TargetActor == this || (OwningCharacter.IsValid() && TargetActor == OwningCharacter.Get()))
	{
		return false;
	}

	return true;
}

void AWeaponBase::ProcessHit(const FHitResult& HitResult)
{
	AActor* TargetActor = HitResult.GetActor();
	if (!TargetActor || !OwningCharacter.IsValid()) return;

	// Build structured damage payload using data-driven config and active attack multiplier
	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage = WeaponConfig.BaseDamage;
	DamageInfo.DamageMultiplier = CurrentDamageMultiplier;
	DamageInfo.PoiseDamage = WeaponConfig.PoiseDamage;
	DamageInfo.KnockbackImpulse = WeaponConfig.KnockbackImpulse;
	DamageInfo.DamageCategory = WeaponConfig.DamageCategory;
	DamageInfo.WeaponType = WeaponConfig.WeaponType;
	DamageInfo.DamageCauser = this;
	DamageInfo.InstigatorWeapon = this;

	// Normalize damage payload ensuring non-negative finite bounds
	DamageInfo.Normalize();

	// Calculate unified raw damage consistently across both handler and native fallback
	const float CalculatedRawDamage = DamageInfo.GetCalculatedRawDamage();

	// 1. Primary: Target implements UDamageHandlerComponent
	UDamageHandlerComponent* TargetHandler = TargetActor->FindComponentByClass<UDamageHandlerComponent>();
	if (TargetHandler)
	{
		TargetHandler->ApplyDamage(OwningCharacter.Get(), TargetActor, DamageInfo, HitResult);
		return;
	}

	// 2. Fallback: Native Unreal Engine TakeDamage
	FPointDamageEvent PointDamageEvent(
		CalculatedRawDamage,
		HitResult,
		HitResult.ImpactNormal,
		nullptr
	);

	TargetActor->TakeDamage(
		CalculatedRawDamage,
		PointDamageEvent,
		OwningCharacter->GetController(),
		this
	);
}
