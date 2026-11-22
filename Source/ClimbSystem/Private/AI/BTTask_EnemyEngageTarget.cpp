// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#include "AI/BTTask_EnemyEngageTarget.h"
#include "AI/EnemyAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "combat/EnemyCharacter.h"
#include "combat/CombatComponent.h"
#include "combat/CombatPositioningComponent.h"
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

	if (Controller && Enemy && (Enemy->bIsInDamageReaction ||
		(Enemy->CombatComponent &&
			(Enemy->CombatComponent->CurrentCombatState == ECombatState::Staggered ||
			 Enemy->CombatComponent->GetAnimationState().DefenseState == ECombatDefenseState::GuardBroken ||
			 Enemy->CombatComponent->IsParryAttemptActive()))))
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
			Enemy->ReleaseAICombatSlot();
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

	if (const ABowEnemyCharacter* Archer = Cast<ABowEnemyCharacter>(Enemy))
	{
		DesiredRange *= FMath::Max(1.0f, Archer->PreferredCombatDistanceMultiplier);
	}

	const FVector EnemyLocation = Enemy->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist2D(EnemyLocation, TargetLocation);
	Controller->SetFocus(Target);
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		Movement->MaxWalkSpeed = Enemy->PursuitWalkSpeed;
	const FVector ToTarget = TargetLocation - EnemyLocation;
	const UCombatPositioningComponent* Positioning =
		Enemy->FindComponentByClass<UCombatPositioningComponent>();
	if (!ToTarget.IsNearlyZero() && (!Positioning || !Positioning->IsAttackPositioningActive()))
	{
		const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
		Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), DesiredRotation, DeltaSeconds, 8.0f));
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
	if (Memory.AttackCooldown <= 0.0f && !Enemy->HasPendingAICombatAction()
		&& Enemy->CombatComponent && Enemy->CombatComponent->CurrentCombatState == ECombatState::Idle)
	{
		if (Enemy->TryAcquireAttackSlot(Target))
		{
			if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
			{
				UE_LOG(LogTemp, Log, TEXT("Task Engage: its mage!!"));
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
				if (Enemy->CombatComponent->IsRiposteWindowActive())
				{
					Enemy->QueueAICombatAction(Target, false);
					if (Enemy->CombatComponent->CurrentCombatState == ECombatState::Idle)
					{
						Enemy->ReleaseAICombatSlot();
					}
				}
				else
				{
					Enemy->ReleaseAICombatSlot();
				}
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
			if (Enemy->CombatComponent)
			{
				Enemy->CombatComponent->CancelAttackInput();
				Enemy->CombatComponent->SetGuardMode(false);
			}
			Enemy->CancelPendingAICombatAction();
			Enemy->ReleaseAICombatSlot();
			if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
				Mage->CancelPendingMagicAttack();
			if (ABowEnemyCharacter* Archer = Cast<ABowEnemyCharacter>(Enemy))
				Archer->CancelPendingBowAttack();
		}
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}
