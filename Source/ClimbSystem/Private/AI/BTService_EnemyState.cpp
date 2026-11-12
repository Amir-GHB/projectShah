// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#include "AI/BTService_EnemyState.h"
#include "AI/EnemyAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "combat/EnemyCharacter.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"

UBTService_EnemyState::UBTService_EnemyState()
{
	NodeName = TEXT("Update Enemy Blackboard");
	Interval = 0.2f;
	RandomDeviation = 0.05f;
}

void UBTService_EnemyState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	AAIController* AI = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AEnemyAIController* EnemyController = Cast<AEnemyAIController>(AI);
	AEnemyCharacter* Enemy = AI ? Cast<AEnemyCharacter>(AI->GetPawn()) : nullptr;
	if (!EnemyController || !Blackboard || !Enemy) return;

	const bool bIsDead = Enemy->DamageHandlerComponent && Enemy->DamageHandlerComponent->bIsDead;
	AActor* Target = bIsDead ? nullptr : EnemyController->GetCurrentTarget();
	const bool bHasTarget = IsValid(Target);
	Blackboard->SetValueAsObject(TargetActorKey, bHasTarget ? Target : nullptr);
	Blackboard->SetValueAsBool(HasTargetKey, bHasTarget);
	Blackboard->SetValueAsVector(InvestigationLocationKey, EnemyController->GetInvestigationLocation());
	Blackboard->SetValueAsBool(HasInvestigationKey, !bIsDead && EnemyController->HasActiveInvestigation());
	Blackboard->SetValueAsBool(IsDeadKey, bIsDead);
	if (bIsDead)
	{
		EnemyController->StopMovement();
		if (Enemy->CombatComponent) Enemy->CombatComponent->CancelAttackInput();
		if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
			Mage->CancelPendingMagicAttack();
	}
}
