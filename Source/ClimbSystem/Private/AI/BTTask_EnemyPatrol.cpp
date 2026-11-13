// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#include "AI/BTTask_EnemyPatrol.h"
#include "AI/EnemyAIController.h"
#include "combat/EnemyCharacter.h"
#include "combat/DamageHandlerComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

uint16 UBTTask_EnemyPatrol::GetInstanceMemorySize() const { return sizeof(FPatrolMemory); }

UBTTask_EnemyPatrol::UBTTask_EnemyPatrol()
{
	NodeName = TEXT("Patrol (Points or Random NavMesh)");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_EnemyPatrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FPatrolMemory& Memory = *reinterpret_cast<FPatrolMemory*>(NodeMemory);
	Memory = FPatrolMemory();
	AEnemyAIController* Controller = Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
	AEnemyCharacter* Enemy = Controller ? Cast<AEnemyCharacter>(Controller->GetPawn()) : nullptr;
	if (!Enemy || !Enemy->bEnablePatrol ||
		(Enemy->PatrolPoints.IsEmpty() && !Enemy->bUseRandomPatrolWhenNoPoints))
		return EBTNodeResult::Failed;
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		Movement->MaxWalkSpeed = Enemy->PatrolWalkSpeed;
	Memory.MontageTimer = FMath::FRandRange(FMath::Max(0.1f, Enemy->PatrolMontageMinInterval),
		FMath::Max(Enemy->PatrolMontageMinInterval, Enemy->PatrolMontageMaxInterval));
	return EBTNodeResult::InProgress;
}

bool UBTTask_EnemyPatrol::ChooseDestination(AEnemyAIController* Controller, AEnemyCharacter* Enemy, FPatrolMemory& Memory) const
{
	if (!IsValid(Controller) || !IsValid(Enemy) || !Enemy->bEnablePatrol) return false;
	if (!Enemy->PatrolPoints.IsEmpty())
	{
		const int32 Count = Enemy->PatrolPoints.Num();
		int32 Index = Controller->GetPatrolPointIndex() % Count;
		for (int32 Attempt = 0; Attempt < Count; ++Attempt)
		{
			if (Enemy->PatrolPoints.IsValidIndex(Index))
			{
				if (AActor* Point = Enemy->PatrolPoints[Index].Get())
				{
					Memory.Destination = Point->GetActorLocation();
					Memory.bHasDestination = true;
					return true;
				}
			}
			Index = (Index + 1) % Count;
			Controller->AdvancePatrolPointIndex(Count);
		}
		return false;
	}
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
	FNavLocation RandomLocation;
	if (!Nav || !Nav->GetRandomReachablePointInRadius(Enemy->GetActorLocation(),
		FMath::Max(100.0f, Enemy->RandomPatrolRadius), RandomLocation))
		return false;
	Memory.Destination = RandomLocation.Location;
	Memory.bHasDestination = true;
	return true;
}

void UBTTask_EnemyPatrol::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FPatrolMemory& Memory = *reinterpret_cast<FPatrolMemory*>(NodeMemory);
	AEnemyAIController* Controller = Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
	AEnemyCharacter* Enemy = Controller ? Cast<AEnemyCharacter>(Controller->GetPawn()) : nullptr;
	if (!Controller || !Enemy || !Enemy->bEnablePatrol ||
		(Enemy->DamageHandlerComponent && Enemy->DamageHandlerComponent->bIsDead))
	{
		if (Controller) Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (Enemy->bIsInDamageReaction)
	{
		Controller->StopMovement();
		return;
	}
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		Movement->MaxWalkSpeed = Enemy->PatrolWalkSpeed;
	Memory.MontageTimer -= DeltaSeconds;
	if (Memory.MontageTimer <= 0.0f)
	{
		if (!Enemy->PatrolMontages.IsEmpty() && Enemy->GetMesh())
		{
			TArray<UAnimMontage*> ValidMontages;
			for (const TObjectPtr<UAnimMontage>& Montage : Enemy->PatrolMontages)
				if (IsValid(Montage)) ValidMontages.Add(Montage.Get());
			if (!ValidMontages.IsEmpty())
			{
				UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
				if (Anim && !Anim->IsAnyMontagePlaying())
					Anim->Montage_Play(ValidMontages[FMath::RandRange(0, ValidMontages.Num() - 1)]);
			}
		}
		Memory.MontageTimer = FMath::FRandRange(FMath::Max(0.1f, Enemy->PatrolMontageMinInterval),
			FMath::Max(Enemy->PatrolMontageMinInterval, Enemy->PatrolMontageMaxInterval));
	}
	if (Memory.bWaiting)
	{
		Memory.WaitRemaining -= DeltaSeconds;
		if (Memory.WaitRemaining <= 0.0f)
		{
			Memory.bWaiting = false;
			Memory.bHasDestination = false;
			if (!Enemy->PatrolPoints.IsEmpty())
				Controller->AdvancePatrolPointIndex(Enemy->PatrolPoints.Num());
		}
		return;
	}
	if (Memory.RetryRemaining > 0.0f)
	{
		Memory.RetryRemaining -= DeltaSeconds;
		return;
	}
	if (!Memory.bHasDestination)
	{
		if (!ChooseDestination(Controller, Enemy, Memory))
		{
			Memory.RetryRemaining = FMath::Max(0.1f, RetryDelay);
			return;
		}
		Controller->MoveToLocation(Memory.Destination, Enemy->PatrolAcceptanceRadius, true, true, true);
		return;
	}
	if (FVector::Dist2D(Enemy->GetActorLocation(), Memory.Destination) <= Enemy->PatrolAcceptanceRadius)
	{
		Controller->StopMovement();
		Memory.bWaiting = true;
		Memory.WaitRemaining = Enemy->PatrolWaitTime;
		return;
	}
	if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		Memory.bHasDestination = false;
		Memory.RetryRemaining = FMath::Max(0.1f, RetryDelay);
	}
}

EBTNodeResult::Type UBTTask_EnemyPatrol::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AEnemyAIController* Controller = Cast<AEnemyAIController>(OwnerComp.GetAIOwner()))
		Controller->StopMovement();
	// Do not overwrite the pursuit/investigation speed when this task is aborted
	// because a higher-priority behavior has taken over.
	return Super::AbortTask(OwnerComp, NodeMemory);
}
