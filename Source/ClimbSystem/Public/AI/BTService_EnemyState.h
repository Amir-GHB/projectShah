// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_EnemyState.generated.h"

/** Mirrors authoritative controller state into the Blackboard. Decision data only; gameplay state stays on components. */
UCLASS()
class CLIMBSYSTEM_API UBTService_EnemyState : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_EnemyState();

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TargetActorKey = TEXT("TargetActor");

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName HasTargetKey = TEXT("HasTarget");

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName InvestigationLocationKey = TEXT("InvestigationLocation");

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName HasInvestigationKey = TEXT("HasInvestigation");

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName IsDeadKey = TEXT("IsDead");

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
