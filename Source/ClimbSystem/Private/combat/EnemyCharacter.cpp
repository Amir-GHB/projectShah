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

namespace
{
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<AEnemyCharacter>>> ActiveAttackersByTarget;
}

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
	if (CombatComponent)
	{
		CombatComponent->OnAttackEnded.AddDynamic(this, &AEnemyCharacter::HandleAICombatAttackEnded);
	}
	if (DamageHandlerComponent)
	{
		DamageHandlerComponent->OnDamageTaken.AddDynamic(this, &AEnemyCharacter::HandleAICombatDamageTaken);
	}
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
	CancelPendingAICombatAction();
	ReleaseAICombatSlot();
	if (CombatComponent)
	{
		CombatComponent->OnAttackEnded.RemoveDynamic(this, &AEnemyCharacter::HandleAICombatAttackEnded);
	}
	if (DamageHandlerComponent)
	{
		DamageHandlerComponent->OnDamageTaken.RemoveDynamic(this, &AEnemyCharacter::HandleAICombatDamageTaken);
	}
	if (ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		if (UCombatComponent* PlayerCombat = PlayerCharacter->FindComponentByClass<UCombatComponent>())
		{
			PlayerCombat->UnregisterEncounterEnemy(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool AEnemyCharacter::TryAcquireAttackSlot(AActor* Target)
{
	if (!IsValid(Target)) return false;
	if (AttackSlotTarget.Get() == Target) return true;
	ReleaseAICombatSlot();

	const TWeakObjectPtr<AActor> TargetKey(Target);
	TArray<TWeakObjectPtr<AEnemyCharacter>>& Slots = ActiveAttackersByTarget.FindOrAdd(TargetKey);
	Slots.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& Entry)
	{
		return !Entry.IsValid();
	});
	if (Slots.Num() >= FMath::Max(1, MaxAttackersPerTarget))
	{
		return false;
	}

	Slots.AddUnique(TWeakObjectPtr<AEnemyCharacter>(this));
	AttackSlotTarget = Target;
	return true;
}

void AEnemyCharacter::ReleaseAICombatSlot()
{
	AActor* Target = AttackSlotTarget.Get();
	AttackSlotTarget.Reset();
	if (!IsValid(Target)) return;

	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (TArray<TWeakObjectPtr<AEnemyCharacter>>* Slots = ActiveAttackersByTarget.Find(TargetKey))
	{
		Slots->RemoveAll([this](const TWeakObjectPtr<AEnemyCharacter>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == this;
		});
		if (Slots->IsEmpty())
		{
			ActiveAttackersByTarget.Remove(TargetKey);
		}
	}
}

void AEnemyCharacter::QueueAICombatAction(AActor* Target, bool bHeavyAttack)
{
	if (!IsValid(Target) || !CombatComponent) return;
	CancelPendingAICombatAction();

	if (bHeavyAttack)
	{
		if (!IsValid(HeavyAttackTelegraphMontage) || !DamageReactionComponent
			|| !DamageReactionComponent->PlayHitReactionOverride(HeavyAttackTelegraphMontage))
		{
			// Never issue an untelegraphed guard-bypassing heavy attack.
			bHeavyAttack = false;
		}
		else
		{
			PendingAICombatTarget = Target;
			bPendingAIHeavyAttack = true;
			const float TelegraphDuration = HeavyAttackTelegraphDuration > 0.0f
				? HeavyAttackTelegraphDuration : HeavyAttackTelegraphMontage->GetPlayLength();
			GetWorldTimerManager().SetTimer(HeavyAttackTelegraphTimer, this,
				&AEnemyCharacter::ReleaseQueuedAICombatAction,
				FMath::Max(0.05f, TelegraphDuration), false);
			return;
		}
	}

	CombatComponent->HandleAICombatAttack(bHeavyAttack);
	if (CombatComponent->CurrentCombatState == ECombatState::Idle)
	{
		ReleaseAICombatSlot();
	}
}

void AEnemyCharacter::ReleaseQueuedAICombatAction()
{
	AActor* Target = PendingAICombatTarget.Get();
	const bool bHeavyAttack = bPendingAIHeavyAttack;
	PendingAICombatTarget.Reset();
	bPendingAIHeavyAttack = false;

	if (!IsValid(Target) || !CombatComponent || (DamageHandlerComponent && DamageHandlerComponent->bIsDead))
	{
		ReleaseAICombatSlot();
		return;
	}

	CombatComponent->HandleAICombatAttack(bHeavyAttack);
	if (CombatComponent->CurrentCombatState == ECombatState::Idle)
	{
		ReleaseAICombatSlot();
	}
}

void AEnemyCharacter::CancelPendingAICombatAction()
{
	GetWorldTimerManager().ClearTimer(HeavyAttackTelegraphTimer);
	PendingAICombatTarget.Reset();
	bPendingAIHeavyAttack = false;
}

void AEnemyCharacter::HandleAICombatAttackEnded()
{
	ReleaseAICombatSlot();
}

void AEnemyCharacter::HandleAICombatDamageTaken(float ActualDamage, AActor* InstigatorActor,
	const FDamageInfo& DamageInfo, const FHitResult& HitResult)
{
	CancelPendingAICombatAction();
	ReleaseAICombatSlot();
}

void AEnemyCharacter::PerformAICombatAction(AActor* Target)
{
	if (CombatComponent) CombatComponent->HandleAttackInput();
}

void AGuardEnemyCharacter::UpdateDefensiveCombat(AActor* Target)
{
	if (!CombatComponent || !IsValid(Target) || !GetWorld()) return;

	const float Distance = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	if (Distance > GuardEngageRange)
	{
		if (CombatComponent->GetAnimationState().DefenseState == ECombatDefenseState::Guarding ||
			CombatComponent->GetAnimationState().DefenseState == ECombatDefenseState::Parrying)
		{
			CombatComponent->SetGuardMode(false);
		}
		return;
	}

	if (CombatComponent->CurrentCombatState == ECombatState::Dead ||
		CombatComponent->CurrentCombatState == ECombatState::Finishing ||
		CombatComponent->CurrentCombatState == ECombatState::Staggered)
	{
		return;
	}

	if (CombatComponent->GetAnimationState().DefenseState == ECombatDefenseState::None)
	{
		CombatComponent->SetGuardMode(true);
	}

	UCombatComponent* TargetCombat = Target->FindComponentByClass<UCombatComponent>();
	if (TargetCombat && TargetCombat->CanBeParried()
		&& GetWorld()->GetTimeSeconds() >= NextParryAttemptTime
		&& FMath::FRand() <= FMath::Clamp(ParryAttemptChance, 0.0f, 1.0f))
	{
		if (CombatComponent->BeginParryAttempt())
		{
			NextParryAttemptTime = GetWorld()->GetTimeSeconds()
				+ FMath::Max(0.05f, ParryAttemptInterval);
		}
	}
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
	ReleaseAICombatSlot();
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
	if (!IsValid(Target) || !IsValid(MagicProjectileClass) || !GetWorld())
	{
		ReleaseAICombatSlot();
		return;
	}
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
	ReleaseAICombatSlot();
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
	ReleaseAICombatSlot();
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
	ReleaseAICombatSlot();
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
