// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/ArrowProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "combat/EnemyCharacter.h"

AArrowProjectile::AArrowProjectile()
{
	// Arrows need a tighter swept collision shape than the generic spell projectile.
	if (CollisionSphere)
	{
		CollisionSphere->SetSphereRadius(2.5f);
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
		CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		CollisionSphere->SetGenerateOverlapEvents(false);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->bSweepCollision = true;
		ProjectileMovement->bShouldBounce = false;
		ProjectileMovement->bRotationFollowsVelocity = true;
		ProjectileMovement->bInitialVelocityInLocalSpace = false;
	}
}

void AArrowProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere)
		CollisionSphere->SetSphereRadius(FMath::Max(1.0f, ProjectileConfig.CollisionRadius));
}

bool AArrowProjectile::IsFriendlyEnemyHit(AActor* OtherActor) const
{
	return IsValid(OtherActor)
		&& Cast<AEnemyCharacter>(DamageInstigator.Get()) != nullptr
		&& Cast<AEnemyCharacter>(OtherActor) != nullptr;
}

void AArrowProjectile::HandleProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (bHasHit || bIsEmbedded || !IsValid(OtherActor))
		return;

	if (OtherActor == DamageInstigator.Get() || OtherActor == SourceWeapon.Get() || OtherActor == GetOwner())
		return;

	// Preserve the project's existing no-friendly-fire rule. Ignore allied enemies
	// and keep flying rather than embedding in or damaging them.
	if (IsFriendlyEnemyHit(OtherActor))
	{
		if (CollisionSphere)
			CollisionSphere->IgnoreActorWhenMoving(OtherActor, true);
		return;
	}

	bHasHit = true;

	if (OtherActor != GetOwner())
		ApplyHitDamage(OtherActor, Hit);

	EmbedAtImpact(OtherComponent, Hit);
}

void AArrowProjectile::EmbedAtImpact(UPrimitiveComponent* OtherComponent, const FHitResult& Hit)
{
	bIsEmbedded = true;
	SetActorTickEnabled(false);

	const FVector TravelDirection = ProjectileMovement
		? ProjectileMovement->Velocity.GetSafeNormal()
		: GetActorForwardVector().GetSafeNormal();

	const FVector Forward = TravelDirection.IsNearlyZero()
		? GetActorForwardVector()
		: TravelDirection;

	// Arrow meshes are expected to point along local +X. The exposed offsets let
	// the mesh pivot and penetration depth be tuned per arrow Blueprint.
	const FVector ImpactLocation = Hit.ImpactPoint
		- Forward * FMath::Max(0.0f, ArrowTipOffset)
		+ Forward * FMath::Max(0.0f, EmbedDepth);

	SetActorLocationAndRotation(ImpactLocation, Forward.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionSphere->SetGenerateOverlapEvents(false);
	}

	// Only the projectile collision is disabled. Keep the arrow mesh visible.
	if (ProjectileVisual)
		ProjectileVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (bAttachToHitComponent && IsValid(OtherComponent))
	{
		FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, true);
		AttachToComponent(OtherComponent, AttachRules, Hit.BoneName);
	}

	SetLifeSpan(FMath::Max(0.0f, EmbeddedLifeSeconds));
}
