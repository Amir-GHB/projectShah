// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#include "AI/BTTask_EnemyEngageTarget.h"
#include "AI/EnemyAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "combat/EnemyCharacter.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

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

	float DesiredRange = Enemy->AttackRange;
	if (const AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
		DesiredRange *= FMath::Max(1.0f, Mage->PreferredCombatDistanceMultiplier);
	if (const ABowEnemyCharacter* Archer = Cast<ABowEnemyCharacter>(Enemy))
		DesiredRange *= FMath::Max(1.0f, Archer->PreferredCombatDistanceMultiplier);

	const FVector EnemyLocation = Enemy->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist2D(EnemyLocation, TargetLocation);
	Controller->SetFocus(Target);

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
	const FVector ToTarget = TargetLocation - EnemyLocation;
	if (!ToTarget.IsNearlyZero())
	{
		const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
		Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), DesiredRotation, DeltaSeconds, 8.0f));
	}

	Memory.AttackCooldown = FMath::Max(0.0f, Memory.AttackCooldown - DeltaSeconds);
	if (Memory.AttackCooldown <= 0.0f && Enemy->CombatComponent &&
		Enemy->CombatComponent->CurrentCombatState == ECombatState::Idle)
	{
		Enemy->PerformAICombatAction(Target);
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
			if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
				Mage->CancelPendingMagicAttack();
		}
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}
