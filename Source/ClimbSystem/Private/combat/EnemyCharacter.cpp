// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/EnemyCharacter.h"

#include "AI/EnemyAIController.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/DamageReactionComponent.h"
#include "MotionWarpingComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "combat/CombatProjectile.h"
#include "Kismet/GameplayStatics.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	DamageHandlerComponent = CreateDefaultSubobject<UDamageHandlerComponent>(TEXT("DamageHandlerComponent"));
	DamageReactionComponent = CreateDefaultSubobject<UDamageReactionComponent>(TEXT("DamageReactionComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->MaxWalkSpeed = 250.0f;
		Movement->NavAgentProps.bCanCrouch = true;
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		if (UCombatComponent* PlayerCombat = PlayerCharacter->FindComponentByClass<UCombatComponent>())
		{
			PlayerCombat->RegisterEncounterEnemy(this);
		}
	}
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		if (UCombatComponent* PlayerCombat = PlayerCharacter->FindComponentByClass<UCombatComponent>())
		{
			PlayerCombat->UnregisterEncounterEnemy(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AEnemyCharacter::PerformAICombatAction(AActor* Target)
{
	if (CombatComponent) CombatComponent->HandleAttackInput();
}

void AMagicEnemyCharacter::PerformAICombatAction(AActor* Target)
{
	if (!IsValid(Target) || !IsValid(MagicProjectileClass) || !GetWorld()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextMagicAttackTime || PendingMagicTarget.IsValid()) return;
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (IsValid(MagicAttackMontage) && Anim && Anim->Montage_IsPlaying(MagicAttackMontage)) return;
	PendingMagicTarget = Target;
	bNeedsRelocationAfterAttack = false;
	if (IsValid(MagicAttackMontage) && Anim && Anim->Montage_Play(MagicAttackMontage) > 0.0f) return;
	ReleaseMagicProjectile();
}

void AMagicEnemyCharacter::CancelPendingMagicAttack()
{
	PendingMagicTarget.Reset();
	if (IsValid(MagicAttackMontage) && GetMesh())
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			if (Anim->Montage_IsPlaying(MagicAttackMontage))
			{
				Anim->Montage_Stop(0.1f, MagicAttackMontage);
			}
		}
	}
}

void AMagicEnemyCharacter::ReleaseMagicProjectile()
{
	AActor* Target = PendingMagicTarget.Get();
	PendingMagicTarget.Reset();
	if (!IsValid(Target) || !IsValid(MagicProjectileClass) || !GetWorld()) return;
	const FVector SpawnLocation = GetMesh() && GetMesh()->DoesSocketExist(ProjectileSpawnSocket)
		? GetMesh()->GetSocketLocation(ProjectileSpawnSocket)
		: GetActorLocation() + GetActorTransform().TransformVectorNoScale(ProjectileSpawnOffset);
	const FVector AimLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
	const FRotator AimRotation = (AimLocation - SpawnLocation).Rotation();
	const FTransform SpawnTransform(AimRotation, SpawnLocation);
	ACombatProjectile* Projectile = GetWorld()->SpawnActorDeferred<ACombatProjectile>(
		MagicProjectileClass, SpawnTransform, this, this,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Projectile)
	{
		FProjectileConfig Config = MagicProjectileConfig;
		Config.GravityScale = 0.0f;
		Projectile->InitializeProjectile(Config, (AimLocation - SpawnLocation).GetSafeNormal(), this, nullptr, Target);
		Projectile->FinishSpawning(SpawnTransform);
	}
	NextMagicAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, MagicAttackCooldown);
	bNeedsRelocationAfterAttack = true;
}

void ABowEnemyCharacter::PerformAICombatAction(AActor* Target)
{
	if (!IsValid(Target) || !CombatComponent || !GetWorld()
		|| CombatComponent->CurrentWeaponType != EWeaponType::Bow
		|| CombatComponent->CurrentCombatState != ECombatState::Idle
		|| GetWorld()->GetTimeSeconds() < NextBowAttackTime)
	{
		return;
	}

	if (AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController()))
	{
		AIController->SetFocus(Target);
	}
	PendingBowTarget = Target;
	CombatComponent->HandleAttackInput();
	if (CombatComponent->CurrentCombatState != ECombatState::DrawingBow)
	{
		PendingBowTarget.Reset();
		return;
	}

	NextBowAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, BowAttackCooldown);
	GetWorldTimerManager().SetTimer(BowReleaseTimer, this, &ABowEnemyCharacter::ReleaseBowShot,
		FMath::Max(0.05f, BowDrawDuration), false);
}

void ABowEnemyCharacter::CancelPendingBowAttack()
{
	GetWorldTimerManager().ClearTimer(BowReleaseTimer);
	PendingBowTarget.Reset();
	if (CombatComponent) CombatComponent->CancelAttackInput();
}

void ABowEnemyCharacter::ReleaseBowShot()
{
	AActor* Target = PendingBowTarget.Get();
	PendingBowTarget.Reset();
	if (!IsValid(Target) || !CombatComponent || CombatComponent->CurrentCombatState != ECombatState::DrawingBow)
	{
		if (CombatComponent) CombatComponent->CancelAttackInput();
		return;
	}
	CombatComponent->HandleAttackReleased();
}

void ABowEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingBowAttack();
	Super::EndPlay(EndPlayReason);
}

float AEnemyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!DamageHandlerComponent || DamageHandlerComponent->bIsDead || ActualDamage <= 0.0f) return ActualDamage;
	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage = ActualDamage;
	DamageInfo.DamageMultiplier = 1.0f;
	DamageInfo.DamageCauser = DamageCauser;
	if (IsValid(DamageCauser))
	{
		DamageInfo.DamageDirection = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
	}
	DamageInfo.Normalize();
	FHitResult HitResult;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		HitResult = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo;
	}
	DamageHandlerComponent->ApplyDamage(DamageCauser, this, DamageInfo, HitResult);
	return ActualDamage;
}
