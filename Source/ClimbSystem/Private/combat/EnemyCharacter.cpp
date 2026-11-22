// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/EnemyCharacter.h"

#include "AI/EnemyAIController.h"
#include "BrainComponent.h"
#include "combat/CombatComponent.h"
#include "combat/CombatPositioningComponent.h"
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
#include "combat/WeaponBase.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<AEnemyCharacter>>> ActiveAttackersByTarget;
}

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CombatPositioningComponent = CreateDefaultSubobject<UCombatPositioningComponent>(TEXT("CombatPositioningComponent"));
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

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsBeingRopePulled)
	{
		SetActorTickEnabled(false);
		return;
	}

	AActor* Puller = RopePuller.Get();
	if (!IsValid(Puller) || (DamageHandlerComponent && DamageHandlerComponent->bIsDead))
	{
		EndRopePull();
		return;
	}

	const FVector ToPuller = Puller->GetActorLocation() - GetActorLocation();
	const FVector Direction = ToPuller.GetSafeNormal2D();
	const float Distance = ToPuller.Size2D();
	if (Distance <= RopePullStopDistance || Direction.IsNearlyZero())
	{
		EndRopePull();
		return;
	}

	const float MoveDistance = FMath::Min(
		FMath::Max(0.0f, RopePullSpeed) * DeltaSeconds,
		Distance - RopePullStopDistance);
	const FRotator FacingRotation(0.0f, Direction.Rotation().Yaw, 0.0f);
	SetActorRotation(FacingRotation);

	FHitResult MoveHit;
	AddActorWorldOffset(Direction * MoveDistance, true, &MoveHit, ETeleportType::None);

	const float RemainingDistance = FVector::Dist2D(GetActorLocation(), Puller->GetActorLocation());
	if (RemainingDistance <= RopePullStopDistance + 2.0f
		|| (MoveHit.IsValidBlockingHit() && RemainingDistance > RopePullStopDistance + 10.0f))
	{
		EndRopePull();
	}
}

bool AEnemyCharacter::BeginRopePull(AActor* Puller, float PullSpeed, float StopDistance)
{
	if (!IsValid(Puller) || Puller == this || bIsBeingRopePulled
		|| bIsInDamageReaction || (DamageHandlerComponent && DamageHandlerComponent->bIsDead))
	{
		return false;
	}
	if (CombatComponent &&
		(CombatComponent->CurrentCombatState == ECombatState::Dead ||
		 CombatComponent->CurrentCombatState == ECombatState::Finishing ||
		 CombatComponent->CurrentCombatState == ECombatState::Staggered))
	{
		return false;
	}

	RopePuller = Puller;
	RopePullSpeed = FMath::Max(0.0f, PullSpeed);
	RopePullStopDistance = FMath::Max(50.0f, StopDistance);

	if (CombatComponent)
	{
		CombatComponent->StopActiveAttack(true, true, 0.0f);
		CombatComponent->SetGuardMode(false);
	}
	CancelPendingAICombatAction();
	if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(this))
	{
		Mage->CancelPendingMagicAttack();
	}
	if (ABowEnemyCharacter* Archer = Cast<ABowEnemyCharacter>(this))
	{
		Archer->CancelPendingBowAttack();
	}
	ReleaseAICombatSlot();

	if (AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController()))
	{
		AIController->StopMovement();
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			bWasRopePullBrainPaused = Brain->IsPaused();
			Brain->PauseLogic(TEXT("Enemy pulled by rope"));
		}
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		PreviousRopePullMovementMode = Movement->MovementMode;
		PreviousRopePullCustomMovementMode = Movement->CustomMovementMode;
		PreviousRopePullWalkSpeed = Movement->MaxWalkSpeed;
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (GetMesh())
	{
		bPreviousRopePullPauseAnims = GetMesh()->bPauseAnims;
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Stop(0.0f);
		}
		GetMesh()->bPauseAnims = true;
	}

	bIsBeingRopePulled = true;
	SetActorTickEnabled(true);
	return true;
}

void AEnemyCharacter::EndRopePull()
{
	if (!bIsBeingRopePulled)
	{
		return;
	}

	bIsBeingRopePulled = false;
	RopePuller.Reset();

	if (GetMesh())
	{
		GetMesh()->bPauseAnims = bPreviousRopePullPauseAnims;
	}

	const bool bDiedDuringPull = DamageHandlerComponent && DamageHandlerComponent->bIsDead;
	if (!bDiedDuringPull)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(PreviousRopePullMovementMode, PreviousRopePullCustomMovementMode);
			Movement->MaxWalkSpeed = PreviousRopePullWalkSpeed;
		}

		if (AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController()))
		{
			if (!bWasRopePullBrainPaused)
			{
				if (UBrainComponent* Brain = AIController->GetBrainComponent())
				{
					Brain->ResumeLogic(TEXT("Rope pull finished"));
				}
			}
		}
	}
	bWasRopePullBrainPaused = false;

	SetActorTickEnabled(false);
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndRopePull();
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
	int32 EffectiveCapacity = FMath::Clamp(MaxAttackersPerTarget, 1, 2);
	for (const TWeakObjectPtr<AEnemyCharacter>& Existing : Slots)
	{
		if (AEnemyCharacter* ExistingEnemy = Existing.Get())
		{
			EffectiveCapacity = FMath::Min(EffectiveCapacity,
				FMath::Clamp(ExistingEnemy->MaxAttackersPerTarget, 1, 2));
		}
	}
	if (Slots.Num() >= EffectiveCapacity)
	{
		return false;
	}

	Slots.AddUnique(TWeakObjectPtr<AEnemyCharacter>(this));
	AttackSlotTarget = Target;
	return true;
}

void AEnemyCharacter::ReleaseAICombatSlot()
{
	AttackSlotTarget.Reset();
	for (auto It = ActiveAttackersByTarget.CreateIterator(); It; ++It)
	{
		It.Value().RemoveAll([this](const TWeakObjectPtr<AEnemyCharacter>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == this;
		});
		if (It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

void AEnemyCharacter::QueueAICombatAction(AActor* Target, bool bHeavyAttack)
{
	if (!IsValid(Target) || !CombatComponent || !IsValid(CombatComponent->CurrentWeapon))
	{
		ReleaseAICombatSlot();
		return;
	}
	CancelPendingAICombatAction();

	if (bHeavyAttack)
	{
		const FWeaponConfig& WeaponConfig = CombatComponent->CurrentWeapon->GetConfig();
		bool bHasHeavyAttack = WeaponConfig.bIsHeavyAttack || !WeaponConfig.bIsLightAttack;
		for (const FAttackDamageConfig& AttackConfig : WeaponConfig.AttackDamageConfigs)
		{
			bHasHeavyAttack = bHasHeavyAttack
				|| (AttackConfig.bOverrideAttackType && AttackConfig.bIsHeavyAttack);
		}
		bHeavyAttack = bHasHeavyAttack;
	}

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

	const UDamageHandlerComponent* TargetDamage = IsValid(Target)
		? Target->FindComponentByClass<UDamageHandlerComponent>() : nullptr;
	if (!IsValid(Target) || (TargetDamage && TargetDamage->bIsDead)
		|| !CombatComponent || (DamageHandlerComponent && DamageHandlerComponent->bIsDead))
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
	(void)ActualDamage;
	(void)InstigatorActor;
	(void)DamageInfo;
	(void)HitResult;
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

	if (CombatComponent->IsRiposteWindowActive())
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
		CombatComponent->BeginParryAttempt();
		NextParryAttemptTime = GetWorld()->GetTimeSeconds()
			+ FMath::Max(0.05f, ParryAttemptInterval);
	}
}

void AMagicEnemyCharacter::PerformAICombatAction(AActor* Target)
{
	UE_LOG(LogTemp, Log, TEXT("mage enemy: PerformAICombatAction"));
	if (!IsValid(Target) || !IsValid(MagicProjectileClass) || !GetWorld()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextMagicAttackTime || PendingMagicTarget.IsValid()) return;
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (IsValid(MagicAttackMontage) && Anim && Anim->Montage_IsPlaying(MagicAttackMontage)) return;
	PendingMagicTarget = Target;
	//bNeedsRelocationAfterAttack = false;
	UE_LOG(LogTemp, Log, TEXT("mage enemy: Right before if"));
	if (IsValid(MagicAttackMontage) && Anim && Anim->Montage_Play(MagicAttackMontage) > 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("mage enemy: inside if"));
		const float FallbackDelay = MagicProjectileFallbackDelay > 0.0f
			? MagicProjectileFallbackDelay : MagicAttackMontage->GetPlayLength();
		GetWorldTimerManager().SetTimer(MagicProjectileFallbackTimer, this,
			&AMagicEnemyCharacter::ReleaseMagicProjectile, FMath::Max(0.05f, FallbackDelay), false);
		return;
	}
	ReleaseMagicProjectile();
}

void AMagicEnemyCharacter::CancelPendingMagicAttack()
{
	GetWorldTimerManager().ClearTimer(MagicProjectileFallbackTimer);
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
	GetWorldTimerManager().ClearTimer(MagicProjectileFallbackTimer);
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
	//bNeedsRelocationAfterAttack = true;
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
		ReleaseAICombatSlot();
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
