// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#include "AI/BTTask_EnemyEngageTarget.h"
#include "AI/EnemyAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "combat/EnemyCharacter.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

uint16 UBTTask_EnemyEngageTarget::GetInstanceMemorySize() const { return sizeof(FEngageMemory); }

UBTTask_EnemyEngageTarget::UBTTask_EnemyEngageTarget()
{
	NodeName = TEXT("Engage Target");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_EnemyEngageTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	*reinterpret_cast<FEngageMemory*>(NodeMemory) = FEngageMemory();
	return EBTNodeResult::InProgress;
}

void UBTTask_EnemyEngageTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FEngageMemory& Memory = *reinterpret_cast<FEngageMemory*>(NodeMemory);
	AEnemyAIController* Controller = Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
	AEnemyCharacter* Enemy = Controller ? Cast<AEnemyCharacter>(Controller->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey)) : nullptr;

	const UDamageHandlerComponent* TargetDamageHandler = IsValid(Target)
		? Target->FindComponentByClass<UDamageHandlerComponent>() : nullptr;
	const bool bTargetDead = TargetDamageHandler && TargetDamageHandler->bIsDead;

	if (Controller && Enemy && Enemy->bIsInDamageReaction)
	{
		Controller->StopMovement();
		return;
	}

	if (!Controller || !Enemy || !IsValid(Target) || bTargetDead ||
		(Enemy->DamageHandlerComponent && Enemy->DamageHandlerComponent->bIsDead))
	{
		if (Controller)
		{
			if (bTargetDead) Controller->ClearCurrentTarget();
			Controller->StopMovement();
		}
		if (Enemy && Enemy->CombatComponent)
		{
			Enemy->CombatComponent->CancelAttackInput();
			Enemy->CombatComponent->StopActiveAttack(true, true, 0.1f);
		}
		if (Enemy) Enemy->CancelPendingAICombatAction();
		if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
		{
			Mage->CancelPendingMagicAttack();
		}
		if (ABowEnemyCharacter* Archer = Cast<ABowEnemyCharacter>(Enemy))
		{
			Archer->CancelPendingBowAttack();
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (AGuardEnemyCharacter* GuardEnemy = Cast<AGuardEnemyCharacter>(Enemy))
	{
		GuardEnemy->UpdateDefensiveCombat(Target);
	}

	float DesiredRange = Enemy->AttackRange;
	if (const AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
		DesiredRange *= FMath::Max(1.0f, Mage->PreferredCombatDistanceMultiplier);
	if (const ABowEnemyCharacter* Archer = Cast<ABowEnemyCharacter>(Enemy))
		DesiredRange *= FMath::Max(1.0f, Archer->PreferredCombatDistanceMultiplier);

	const FVector EnemyLocation = Enemy->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist2D(EnemyLocation, TargetLocation);
	Controller->SetFocus(Target);
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		Movement->MaxWalkSpeed = Enemy->PursuitWalkSpeed;
	const FVector ToTarget = TargetLocation - EnemyLocation;
	if (!ToTarget.IsNearlyZero())
	{
		const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
		Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), DesiredRotation, DeltaSeconds, 8.0f));
	}

	if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
	{
		if (Mage->bNeedsRelocationAfterAttack)
		{
			if (!Memory.bRelocationIssued)
			{
				UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
				FNavLocation Candidate;
				bool bFound = false;
				const float Tolerance = FMath::Max(100.0f, DesiredRange * 0.25f);
				for (int32 Attempt = 0; Nav && Attempt < 12; ++Attempt)
				{
					if (Nav->GetRandomReachablePointInRadius(EnemyLocation, 420.0f, Candidate) &&
						FMath::Abs(FVector::Dist2D(Candidate.Location, TargetLocation) - DesiredRange) <= Tolerance)
					{
						bFound = true;
						break;
					}
				}
				if (!bFound && Nav)
				{
					const FVector Away = (EnemyLocation - TargetLocation).GetSafeNormal2D();
					const FVector Projected = EnemyLocation + (Away.IsNearlyZero() ? -Enemy->GetActorForwardVector() : Away) * 300.0f;
					Nav->ProjectPointToNavigation(Projected, Candidate);
				}
				if (!Nav)
				{
					Mage->bNeedsRelocationAfterAttack = false;
					return;
				}
				Memory.RelocationDestination = Candidate.Location;
				Memory.bRelocationIssued = true;
				Controller->MoveToLocation(Memory.RelocationDestination, 80.0f);
			}
			if (FVector::Dist2D(EnemyLocation, Memory.RelocationDestination) <= 100.0f ||
				Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
			{
				Mage->bNeedsRelocationAfterAttack = false;
				Memory.bRelocationIssued = false;
				Memory.bHasMoveGoal = false;
			}
			return;
		}
	}

	if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
	{
		Memory.StrafeCooldown = FMath::Max(0.0f, Memory.StrafeCooldown - DeltaSeconds);
		if (Distance <= DesiredRange * 1.15f)
		{
			if (Memory.bStrafeMoveIssued)
			{
				if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle ||
					FVector::Dist2D(Enemy->GetActorLocation(), Memory.RelocationDestination) <= 90.0f)
				{
					Memory.bStrafeMoveIssued = false;
					Memory.StrafeCooldown = FMath::FRandRange(0.4f, 1.0f);
				}
				return;
			}
			if (Memory.StrafeCooldown <= 0.0f)
			{
				UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
				if (Nav)
				{
					const FVector Away = (EnemyLocation - TargetLocation).GetSafeNormal2D();
					const FVector Right = FVector::CrossProduct(FVector::UpVector, Away).GetSafeNormal2D();
					const FVector Direction = Distance < DesiredRange * 0.82f ? Away : (FMath::RandBool() ? Right : -Right);
					const FVector DesiredPoint = EnemyLocation + Direction * FMath::FRandRange(220.0f, 340.0f);
					FNavLocation ProjectedPoint;
					if (Nav->ProjectPointToNavigation(DesiredPoint, ProjectedPoint))
					{
						Memory.RelocationDestination = ProjectedPoint.Location;
						Memory.bStrafeMoveIssued = true;
						Controller->MoveToLocation(Memory.RelocationDestination, 65.0f);
						return;
					}
				}
				Memory.StrafeCooldown = 0.8f;
			}
			Controller->StopMovement();
			return;
		}
		Memory.bStrafeMoveIssued = false;
	}

	if (Distance > DesiredRange)
	{
		if (!Memory.bHasMoveGoal ||
			FVector::DistSquared2D(Memory.LastMoveGoal, TargetLocation) > FMath::Square(MoveUpdateDistance) ||
			Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			Controller->MoveToActor(Target, FMath::Max(60.0f, DesiredRange * 0.85f), true, true, true);
			Memory.LastMoveGoal = TargetLocation;
			Memory.bHasMoveGoal = true;
		}
		return;
	}

	Controller->StopMovement();
	Memory.bHasMoveGoal = false;

	Memory.AttackCooldown = FMath::Max(0.0f, Memory.AttackCooldown - DeltaSeconds);
	if (Memory.AttackCooldown <= 0.0f && Enemy->CombatComponent &&
		Enemy->CombatComponent->CurrentCombatState == ECombatState::Idle)
	{
		if (Enemy->TryAcquireAttackSlot(Target))
		{
			if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
			{
				Mage->PerformAICombatAction(Target);
				if (!Mage->HasPendingMagicAttack()) Enemy->ReleaseAICombatSlot();
			}
			else if (ABowEnemyCharacter* Archer = Cast<ABowEnemyCharacter>(Enemy))
			{
				Archer->PerformAICombatAction(Target);
				if (!Archer->HasPendingBowAttack()) Enemy->ReleaseAICombatSlot();
			}
			else if (!Cast<AGuardEnemyCharacter>(Enemy))
			{
				const bool bRequestHeavy = FMath::FRand() <= FMath::Clamp(Enemy->HeavyAttackChance, 0.0f, 1.0f);
				Enemy->QueueAICombatAction(Target, bRequestHeavy);
				if (!Enemy->HasPendingAICombatAction()
					&& Enemy->CombatComponent->CurrentCombatState == ECombatState::Idle)
				{
					Enemy->ReleaseAICombatSlot();
				}
			}
			else
			{
				Enemy->ReleaseAICombatSlot();
			}
		}
		Memory.AttackCooldown = FMath::Max(0.05f, Enemy->AttackRetryInterval);
	}
}

EBTNodeResult::Type UBTTask_EnemyEngageTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AEnemyAIController* Controller = Cast<AEnemyAIController>(OwnerComp.GetAIOwner()))
	{
		Controller->StopMovement();
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Controller->GetPawn()))
		{
			if (Enemy->CombatComponent) Enemy->CombatComponent->CancelAttackInput();
			Enemy->CancelPendingAICombatAction();
			Enemy->ReleaseAICombatSlot();
			if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
				Mage->CancelPendingMagicAttack();
		}
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}
