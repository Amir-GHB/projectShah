// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Bow.h"
#include "combat/CombatProjectile.h"
#include "Components/SkeletalMeshComponent.h"
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

	ProjectileClass = ACombatProjectile::StaticClass();
	ProjectileConfig.BaseDamage = WeaponConfig.BaseDamage;
	ProjectileConfig.PoiseDamage = WeaponConfig.PoiseDamage;
	ProjectileConfig.KnockbackImpulse = WeaponConfig.KnockbackImpulse;
	ProjectileConfig.DamageCategory = WeaponConfig.DamageCategory;
	ProjectileConfig.WeaponType = EWeaponType::Bow;
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
	PlayCharacterMontage(DrawMontage);
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
	DrawAlpha = 0.0f;
	PlayCharacterMontage(ReleaseMontage);
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
	if (IsValid(DrawMontage) && OwningCharacter.IsValid() && OwningCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Stop(0.12f, DrawMontage);
		}
	}
	OnBowDrawStateChanged.Broadcast(false, 0.0f);
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
	ShotConfig.DamageMultiplier *= FMath::Lerp(MinDrawDamageMultiplier, MaxDrawDamageMultiplier, ChargeAlpha);
	ShotConfig.InitialSpeed *= FMath::Lerp(MinDrawSpeedMultiplier, MaxDrawSpeedMultiplier, ChargeAlpha);
	ShotConfig.MaxSpeed = FMath::Max(ShotConfig.MaxSpeed, ShotConfig.InitialSpeed);

	APawn* InstigatorPawn = Cast<APawn>(Character);
	ACombatProjectile* Projectile = World->SpawnActorDeferred<ACombatProjectile>(
		ProjectileClass,
		SpawnTransform,
		Character,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Projectile) return false;

	Projectile->InitializeProjectile(ShotConfig, Direction, Character, this);
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
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

	FVector AimOrigin = Character->GetPawnViewLocation();
	FRotator AimRotation = Character->GetActorRotation();
	if (AController* Controller = Character->GetController())
	{
		AimRotation = Controller->GetControlRotation();
	}

	const FVector AimEnd = AimOrigin + AimRotation.Vector() * 10000.0f;
	FVector AimTarget = AimEnd;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BowAimTrace), true, Character);
	QueryParams.AddIgnoredActor(this);
	FHitResult AimHit;
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(AimHit, AimOrigin, AimEnd, ECC_Visibility, QueryParams) && AimHit.bBlockingHit)
	{
		AimTarget = AimHit.ImpactPoint;
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
