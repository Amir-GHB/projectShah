// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Bow.h"
#include "riding/RidingComponent.h"
#include "riding/HorseCharacter.h"
#include "AI/EnemyAIController.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/CombatProjectile.h"
#include "combat/ArrowProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AWeapon_Bow::AWeapon_Bow()
{
	PrimaryActorTick.bCanEverTick = true;

	WeaponConfig.WeaponType = EWeaponType::Bow;
	WeaponConfig.WeaponDisplayName = FText::FromString(TEXT("Bow"));
	WeaponConfig.BaseDamage = 30.0f;
	WeaponConfig.PoiseDamage = 10.0f;
	WeaponConfig.KnockbackImpulse = 250.0f;
	WeaponConfig.DamageCategory = EDamageCategory::Piercing;
	WeaponConfig.HandSocketName = FName("Bow_Hand_Socket");
	WeaponConfig.HolsterSocketName = FName("Bow_Holster_Socket");
	WeaponConfig.TraceSocketNames.Empty();

	WeaponMesh->SetVisibility(false, true);
	SkeletalBowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalBowMesh"));
	SkeletalBowMesh->SetupAttachment(RootSceneComponent);
	SkeletalBowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalBowMesh->SetGenerateOverlapEvents(false);

	ProjectileClass = AArrowProjectile::StaticClass();
	ProjectileConfig.BaseDamage = WeaponConfig.BaseDamage;
	ProjectileConfig.PoiseDamage = WeaponConfig.PoiseDamage;
	ProjectileConfig.KnockbackImpulse = WeaponConfig.KnockbackImpulse;
	ProjectileConfig.DamageCategory = WeaponConfig.DamageCategory;
	ProjectileConfig.WeaponType = EWeaponType::Bow;
	ProjectileConfig.CriticalChance = WeaponConfig.CriticalChance;
	ProjectileConfig.CriticalDamageMultiplier = WeaponConfig.CriticalDamageMultiplier;
	ProjectileConfig.DamageOverTimeEffects = WeaponConfig.DamageOverTimeEffects;
}

void AWeapon_Bow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bIsDrawing || !GetWorld()) return;

	const float Duration = FMath::Max(0.05f, FullDrawTime);
	const float NewAlpha = FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - DrawStartTime) / Duration), 0.0f, 1.0f);
	DrawAlpha = NewAlpha;
}

bool AWeapon_Bow::BeginDraw()
{
	if (bIsDrawing || !GetWorld()) return false;

	bIsDrawing = true;
	DrawAlpha = 0.0f;
	DrawStartTime = GetWorld()->GetTimeSeconds();
	SpawnInHandArrow();
	UAnimMontage* DrawToPlay = DrawMontage;
	if (ACharacter* Character = OwningCharacter.Get())
	{
		if (URidingComponent* Riding = Character->FindComponentByClass<URidingComponent>())
		{
			if (Riding->IsMounted() && IsValid(MountedDrawMontage)) DrawToPlay = MountedDrawMontage;
		}
	}
	ActiveDrawMontage = DrawToPlay;
	PlayCharacterMontage(DrawToPlay);
	OnBowDrawStateChanged.Broadcast(true, DrawAlpha);
	return true;
}

bool AWeapon_Bow::ReleaseShot()
{
	if (!bIsDrawing || !GetWorld()) return false;

	const float HeldSeconds = FMath::Max(0.0f, static_cast<float>(GetWorld()->GetTimeSeconds() - DrawStartTime));
	if (!bAllowQuickShot && HeldSeconds < FMath::Max(0.0f, MinimumDrawTime))
	{
		CancelDraw();
		return false;
	}

	const float ChargeAlpha = FMath::Clamp(HeldSeconds / FMath::Max(0.05f, FullDrawTime), 0.0f, 1.0f);
	const bool bFired = SpawnArrow(ChargeAlpha);
	DestroyInHandArrow();
	DrawAlpha = 0.0f;
	UAnimMontage* ReleaseToPlay = ReleaseMontage;
	if (ACharacter* Character = OwningCharacter.Get())
	{
		if (URidingComponent* Riding = Character->FindComponentByClass<URidingComponent>())
		{
			if (Riding->IsMounted() && IsValid(MountedReleaseMontage)) ReleaseToPlay = MountedReleaseMontage;
		}
	}
	PlayCharacterMontage(ReleaseToPlay);
	ActiveDrawMontage = nullptr;
	//change drawing state after montage is played to pervent flicking between non aim state and release montage
	bIsDrawing = false;
	OnBowDrawStateChanged.Broadcast(false, 0.0f);
	return bFired;
}

void AWeapon_Bow::CancelDraw()
{
	if (!bIsDrawing) return;

	bIsDrawing = false;
	DrawAlpha = 0.0f;
	DestroyInHandArrow();
	if (IsValid(ActiveDrawMontage) && OwningCharacter.IsValid() && OwningCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Stop(0.12f, ActiveDrawMontage);
		}
	}
	ActiveDrawMontage = nullptr;
	OnBowDrawStateChanged.Broadcast(false, 0.0f);
}

void AWeapon_Bow::SpawnInHandArrow()
{
	DestroyInHandArrow();

	ACharacter* Character = OwningCharacter.Get();
	USkeletalMeshComponent* CharacterMesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	if (!IsValid(CharacterMesh) || !IsValid(InHandArrowMesh))
		return;

	if (!CharacterMesh->DoesSocketExist(InHandArrowSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Bow] In-hand arrow socket/bone '%s' was not found on character mesh '%s'."),
			*InHandArrowSocketName.ToString(), *GetNameSafe(CharacterMesh->GetSkeletalMeshAsset()));
		return;
	}

	ActiveInHandArrow = NewObject<UStaticMeshComponent>(this, TEXT("ActiveInHandArrow"));
	if (!IsValid(ActiveInHandArrow))
		return;

	ActiveInHandArrow->SetStaticMesh(InHandArrowMesh);
	ActiveInHandArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ActiveInHandArrow->SetGenerateOverlapEvents(false);
	ActiveInHandArrow->SetCastShadow(true);
	ActiveInHandArrow->RegisterComponent();
	ActiveInHandArrow->AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		InHandArrowSocketName);
	ActiveInHandArrow->SetRelativeTransform(InHandArrowRelativeTransform);
}

void AWeapon_Bow::DestroyInHandArrow()
{
	if (IsValid(ActiveInHandArrow))
	{
		ActiveInHandArrow->DestroyComponent();
	}
	ActiveInHandArrow = nullptr;
}

bool AWeapon_Bow::SpawnArrow(float ChargeAlpha)
{
	ACharacter* Character = OwningCharacter.Get();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World || !IsValid(ProjectileClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Bow] Cannot fire: character, world, or projectile class is missing."));
		return false;
	}

	FVector Direction;
	FTransform SpawnTransform;
	if (!GetAimDirectionAndSpawnTransform(Direction, SpawnTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Bow] Could not resolve a valid projectile spawn direction."));
		return false;
	}

	FProjectileConfig ShotConfig = ProjectileConfig;
	ShotConfig.BaseDamage = WeaponConfig.BaseDamage;
	ShotConfig.PoiseDamage = WeaponConfig.PoiseDamage;
	ShotConfig.KnockbackImpulse = WeaponConfig.KnockbackImpulse;
	ShotConfig.DamageCategory = WeaponConfig.DamageCategory;
	ShotConfig.WeaponType = EWeaponType::Bow;
	ShotConfig.CriticalChance = WeaponConfig.CriticalChance;
	ShotConfig.CriticalDamageMultiplier = WeaponConfig.CriticalDamageMultiplier;
	ShotConfig.DamageOverTimeEffects = WeaponConfig.DamageOverTimeEffects;
	ShotConfig.DamageMultiplier *= FMath::Lerp(MinDrawDamageMultiplier, MaxDrawDamageMultiplier, ChargeAlpha);
	ShotConfig.InitialSpeed *= FMath::Lerp(MinDrawSpeedMultiplier, MaxDrawSpeedMultiplier, ChargeAlpha);
	ShotConfig.MaxSpeed = FMath::Max(ShotConfig.MaxSpeed, ShotConfig.InitialSpeed);

	// A bow must never fall back to the generic spell projectile. Keep the
	// configurable base-class property for existing Blueprint defaults, but
	// transparently replace legacy generic projectile assignments with the
	// dedicated arrow implementation.
	TSubclassOf<ACombatProjectile> ResolvedProjectileClass = ProjectileClass;
	if (!ResolvedProjectileClass || !ResolvedProjectileClass->IsChildOf(AArrowProjectile::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Bow] ProjectileClass is not an AArrowProjectile. Using the native arrow class instead."));
		ResolvedProjectileClass = AArrowProjectile::StaticClass();
	}

	APawn* InstigatorPawn = Cast<APawn>(Character);
	ACombatProjectile* SpawnedProjectile = World->SpawnActorDeferred<ACombatProjectile>(
		ResolvedProjectileClass,
		SpawnTransform,
		Character,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	AArrowProjectile* Arrow = Cast<AArrowProjectile>(SpawnedProjectile);
	if (!Arrow)
	{
		if (IsValid(SpawnedProjectile)) SpawnedProjectile->Destroy();
		return false;
	}

	Arrow->InitializeProjectile(ShotConfig, Direction, Character, this);
	UGameplayStatics::FinishSpawningActor(Arrow, SpawnTransform);
	return true;
}

bool AWeapon_Bow::GetAimDirectionAndSpawnTransform(FVector& OutDirection, FTransform& OutSpawnTransform) const
{
	ACharacter* Character = OwningCharacter.Get();
	if (!IsValid(Character)) return false;

	FTransform SocketTransform;
	bool bFoundSpawnSocket = false;
	if (IsValid(SkeletalBowMesh) && SkeletalBowMesh->DoesSocketExist(ArrowSpawnSocketName))
	{
		SocketTransform = SkeletalBowMesh->GetSocketTransform(ArrowSpawnSocketName, RTS_World);
		bFoundSpawnSocket = true;
	}
	else if (IsValid(WeaponMesh) && WeaponMesh->DoesSocketExist(ArrowSpawnSocketName))
	{
		SocketTransform = WeaponMesh->GetSocketTransform(ArrowSpawnSocketName, RTS_World);
		bFoundSpawnSocket = true;
	}

	if (!bFoundSpawnSocket)
	{
		const FVector FallbackLocation = Character->GetPawnViewLocation() + Character->GetActorForwardVector() * 45.0f;
		SocketTransform = FTransform(Character->GetActorRotation(), FallbackLocation);
	}

	AController* AimController = Character->GetController();
	if (!AimController)
	{
		if (URidingComponent* Riding = Character->FindComponentByClass<URidingComponent>())
		{
			if (AHorseCharacter* Horse = Riding->GetCurrentHorse()) AimController = Horse->GetController();
		}
	}
	const APawn* ViewPawn = AimController ? AimController->GetPawn() : Character;
	const FVector AimOrigin = IsValid(ViewPawn) ? ViewPawn->GetPawnViewLocation() : Character->GetPawnViewLocation();
	FRotator AimRotation = Character->GetActorRotation();
	FVector AimTarget = FVector::ZeroVector;
	bool bHasAITarget = false;

	// Player characters aim through their camera trace. AI has no player camera, so
	// resolve its current perception target and aim at the target's upper body instead.
	if (const AEnemyAIController* AIController = Cast<AEnemyAIController>(Character->GetController()))
	{
		if (AActor* Target = AIController->GetCurrentTarget())
		{
			const UDamageHandlerComponent* TargetDamage = Target->FindComponentByClass<UDamageHandlerComponent>();
			if (!TargetDamage || !TargetDamage->bIsDead)
			{
				if (const ACharacter* TargetCharacter = Cast<ACharacter>(Target))
				{
					if (const USkeletalMeshComponent* TargetMesh = TargetCharacter->GetMesh())
					{
						const FBox MeshBounds = TargetMesh->Bounds.GetBox();
						if (MeshBounds.IsValid)
						{
							AimTarget = MeshBounds.GetCenter();
							AimTarget.Z = FMath::Lerp(MeshBounds.Min.Z, MeshBounds.Max.Z, 0.68f);
							bHasAITarget = !AimTarget.ContainsNaN();
						}
					}
				}
				if (!bHasAITarget)
				{
					FVector TargetBoundsOrigin;
					FVector TargetBoundsExtent;
					Target->GetActorBounds(false, TargetBoundsOrigin, TargetBoundsExtent);
					AimTarget = TargetBoundsOrigin + FVector(0.0f, 0.0f, TargetBoundsExtent.Z * 0.25f);
					bHasAITarget = !AimTarget.ContainsNaN();
				}
				if (bHasAITarget)
					AimRotation = (AimTarget - AimOrigin).Rotation();
			}
		}
	}

	if (!bHasAITarget)
	{
		if (AimController)
		{
			AimRotation = AimController->GetControlRotation();
		}

		const FVector AimEnd = AimOrigin + AimRotation.Vector() * 10000.0f;
		AimTarget = AimEnd;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BowAimTrace), true, Character);
		QueryParams.AddIgnoredActor(this);
		FHitResult AimHit;
		if (GetWorld() && GetWorld()->LineTraceSingleByChannel(AimHit, AimOrigin, AimEnd, ECC_Visibility, QueryParams) && AimHit.bBlockingHit)
		{
			AimTarget = AimHit.ImpactPoint;
		}
	}

	const FVector SpawnLocation = SocketTransform.GetLocation();
	OutDirection = (AimTarget - SpawnLocation).GetSafeNormal();
	if (OutDirection.IsNearlyZero())
	{
		OutDirection = AimRotation.Vector().GetSafeNormal();
	}
	if (OutDirection.IsNearlyZero()) return false;

	OutSpawnTransform = FTransform(OutDirection.Rotation(), SpawnLocation);
	return true;
}
void AWeapon_Bow::PlayCharacterMontage(UAnimMontage* Montage)
{
	if (!IsValid(Montage) || !OwningCharacter.IsValid() || !OwningCharacter->GetMesh()) return;
	if (UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance())
	{
		Anim->Montage_Play(Montage);
	}
}
