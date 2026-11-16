// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/ArrowProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
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

	// The base class binds its generic destroy-on-hit handler. Replace that
	// delegate explicitly so this subclass owns the complete impact lifecycle.
	if (CollisionSphere)
	{
		CollisionSphere->OnComponentHit.RemoveDynamic(this, &ACombatProjectile::HandleProjectileHit);
		CollisionSphere->OnComponentHit.AddDynamic(this, &AArrowProjectile::HandleProjectileHit);
		CollisionSphere->SetSphereRadius(FMath::Max(1.0f, ProjectileConfig.CollisionRadius));
	}
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

	if (bAttachToHitComponent)
	{
		USkeletalMeshComponent* TargetSkeletalMesh = Cast<USkeletalMeshComponent>(OtherComponent);
		if (!TargetSkeletalMesh)
		{
			if (ACharacter* HitCharacter = Cast<ACharacter>(Hit.GetActor()))
			{
				TargetSkeletalMesh = HitCharacter->GetMesh();
			}
			else if (IsValid(Hit.GetActor()))
			{
				TArray<USkeletalMeshComponent*> SkeletalMeshes;
				Hit.GetActor()->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);
				float BestDistanceSquared = TNumericLimits<float>::Max();
				for (USkeletalMeshComponent* Candidate : SkeletalMeshes)
				{
					if (!IsValid(Candidate)) continue;
					const float DistanceSquared = FVector::DistSquared(Candidate->GetComponentLocation(), Hit.ImpactPoint);
					if (DistanceSquared < BestDistanceSquared)
					{
						BestDistanceSquared = DistanceSquared;
						TargetSkeletalMesh = Candidate;
					}
				}
			}
		}

		USceneComponent* AttachComponent = TargetSkeletalMesh
			? Cast<USceneComponent>(TargetSkeletalMesh)
			: OtherComponent;
		FName AttachBoneName = NAME_None;

		// Capsule collision often reports no bone. Resolve the closest actual mesh
		// bone so the arrow follows hit reactions and individual ragdoll bodies.
		if (TargetSkeletalMesh && TargetSkeletalMesh->GetSkeletalMeshAsset())
		{
			AttachBoneName = Hit.BoneName;
			if (AttachBoneName.IsNone() || TargetSkeletalMesh->GetBoneIndex(AttachBoneName) == INDEX_NONE)
			{
				float BestBoneDistanceSquared = TNumericLimits<float>::Max();
				const int32 BoneCount = TargetSkeletalMesh->GetNumBones();
				for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
				{
					const FName CandidateBone = TargetSkeletalMesh->GetBoneName(BoneIndex);
					const FVector BoneLocation = TargetSkeletalMesh->GetBoneLocation(CandidateBone);
					const float DistanceSquared = FVector::DistSquared(BoneLocation, Hit.ImpactPoint);
					if (DistanceSquared < BestBoneDistanceSquared)
					{
						BestBoneDistanceSquared = DistanceSquared;
						AttachBoneName = CandidateBone;
					}
				}
			}
		}

		if (IsValid(AttachComponent))
		{
			FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, true);
			AttachToComponent(AttachComponent, AttachRules, AttachBoneName);
		}
	}

	SetLifeSpan(FMath::Max(0.0f, EmbeddedLifeSeconds));
}
