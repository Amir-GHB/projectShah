// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/ThrowableTreeActor.h"

#include "combat/CombatCharacterBase.h"
#include "combat/EnemyCharacter.h"
#include "combat/DamageHandlerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

AThrowableTreeActor::AThrowableTreeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetNotifyRigidBodyCollision(true);
	MeshComponent->SetGenerateOverlapEvents(false);
}

void AThrowableTreeActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(TreeMesh))
	{
		MeshComponent->SetStaticMesh(TreeMesh);
	}
	MeshComponent->OnComponentHit.AddDynamic(this, &AThrowableTreeActor::HandleMeshHit);
}

bool AThrowableTreeActor::BeginThrowSequence(ACombatCharacterBase* Thrower)
{
	if (!CanBeThrown() || !IsValid(Thrower) || !IsValid(ThrowMontage) || !Thrower->GetMesh()) return false;
	UAnimInstance* Anim = Thrower->GetMesh()->GetAnimInstance();
	if (!Anim) return false;
	Thrower->SetPendingThrowable(this);
	if (Anim->Montage_Play(ThrowMontage) <= 0.0f)
	{
		Thrower->CancelPendingThrowable();
		return false;
	}
	return true;
}

bool AThrowableTreeActor::AttachToThrowerHand(ACombatCharacterBase* Thrower)
{
	if (!CanBeThrown() || !IsValid(Thrower) || !Thrower->GetMesh()
		|| !Thrower->GetMesh()->DoesSocketExist(ThrowerHandSocketName)
		|| !MeshComponent->DoesSocketExist(ThrowableGripSocketName)) return false;

	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
	MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	const FTransform GripSocketLocal = MeshComponent->GetSocketTransform(ThrowableGripSocketName, RTS_Component);
	if (!AttachToComponent(Thrower->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, ThrowerHandSocketName)) return false;
	SetActorRelativeTransform(GripSocketLocal.Inverse());
	bIsAttachedToHand = true;
	ThrowingCharacter = Thrower;
	SetOwner(Thrower);
	return true;
}

bool AThrowableTreeActor::ReleaseFromThrowerHand(ACombatCharacterBase* Thrower)
{
	if (!bIsAttachedToHand || !IsValid(Thrower) || ThrowingCharacter.Get() != Thrower) return false;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bIsAttachedToHand = false;
	const bool bLaunched = ThrowAtClosestVisibleEnemy(Thrower);
	if (!bLaunched && IsValid(MeshComponent))
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->WakeAllRigidBodies();
	}
	return bLaunched;
}

bool AThrowableTreeActor::ThrowAtClosestVisibleEnemy(ACombatCharacterBase* Thrower)
{
	if (!CanBeThrown() || !IsValid(Thrower) || !GetWorld())
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	Thrower->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	if (IsValid(Thrower->FollowCamera))
	{
		ViewLocation = Thrower->FollowCamera->GetComponentLocation();
		ViewRotation = Thrower->FollowCamera->GetComponentRotation();
	}
	else if (IsValid(Thrower->GetController()))
	{
		ViewRotation = Thrower->GetController()->GetControlRotation();
	}

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	constexpr float MinimumViewDot = 0.88f;
	AEnemyCharacter* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ThrowableTreeTargetSight), true);
	QueryParams.AddIgnoredActor(Thrower);
	QueryParams.AddIgnoredActor(this);

	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->DamageHandlerComponent
			|| Candidate->DamageHandlerComponent->bIsDead || Candidate->bIsInDamageReaction)
		{
			continue;
		}

		const FVector TargetPoint = Candidate->GetActorLocation()
			+ FVector(0.0f, 0.0f, Candidate->GetCapsuleComponent()
				? Candidate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.35f : 50.0f);
		const FVector ToTarget = TargetPoint - ViewLocation;
		const float DistanceSquared = ToTarget.SizeSquared();
		if (DistanceSquared <= KINDA_SMALL_NUMBER
			|| FVector::DotProduct(ViewDirection, ToTarget.GetSafeNormal()) < MinimumViewDot
			|| DistanceSquared >= BestDistanceSquared)
		{
			continue;
		}

		FHitResult SightHit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			SightHit, ViewLocation, TargetPoint, ECC_Visibility, QueryParams);
		if (bBlocked && SightHit.GetActor() != Candidate)
		{
			continue;
		}

		BestTarget = Candidate;
		BestDistanceSquared = DistanceSquared;
	}

	if (!IsValid(BestTarget))
	{
		return false;
	}

	const FVector TargetPoint = BestTarget->GetActorLocation()
		+ FVector(0.0f, 0.0f, BestTarget->GetCapsuleComponent()
			? BestTarget->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.35f : 50.0f);
	FVector LaunchDirection = (TargetPoint - MeshComponent->GetComponentLocation()).GetSafeNormal();
	LaunchDirection = (LaunchDirection + FVector::UpVector * FMath::Max(0.0f, UpwardArc)).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		return false;
	}

	bHasBeenThrown = true;
	bIsAttachedToHand = false;
	ThrowingCharacter = Thrower;
	SetOwner(Thrower);
	MeshComponent->IgnoreActorWhenMoving(Thrower, true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();
	MeshComponent->SetPhysicsLinearVelocity(LaunchDirection * FMath::Max(100.0f, ThrowSpeed));
	SetLifeSpan(FMath::Max(0.1f, ThrownLifeSeconds));
	return true;
}

void AThrowableTreeActor::HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bHasBeenThrown || !IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(OtherActor))
	{
		if (Enemy->DamageHandlerComponent && !Enemy->DamageHandlerComponent->bIsDead)
		{
			FDamageInfo DamageInfo;
			DamageInfo.BaseDamage = ImpactDamage;
			DamageInfo.DamageMultiplier = 1.0f;
			DamageInfo.PoiseDamage = ImpactPoiseDamage;
			DamageInfo.KnockbackImpulse = MeshComponent->GetPhysicsLinearVelocity().Size();
			DamageInfo.DamageCategory = EDamageCategory::Blunt;
			DamageInfo.WeaponType = EWeaponType::CustomWeapon;
			DamageInfo.DamageCauser = this;
			DamageInfo.DamageDirection = MeshComponent->GetPhysicsLinearVelocity().GetSafeNormal();
			DamageInfo.Normalize();
			Enemy->DamageHandlerComponent->ApplyDamage(
				ThrowingCharacter.Get(), Enemy, DamageInfo, Hit);
		}
	}

	StopAfterImpact();
}

void AThrowableTreeActor::StopAfterImpact()
{
	if (!bHasBeenThrown)
	{
		return;
	}

	if (IsValid(MeshComponent))
	{
		MeshComponent->SetNotifyRigidBodyCollision(false);
		MeshComponent->SetSimulatePhysics(false);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	SetLifeSpan(2.0f);
}
