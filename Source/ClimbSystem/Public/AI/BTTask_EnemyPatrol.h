// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_EnemyPatrol.generated.h"

class AEnemyCharacter;
class AEnemyAIController;

UCLASS()
class CLIMBSYSTEM_API UBTTask_EnemyPatrol : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_EnemyPatrol();
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float RetryDelay = 1.0f;
	virtual uint16 GetInstanceMemorySize() const override;
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
private:
	struct FPatrolMemory
	{
		FVector Destination = FVector::ZeroVector;
		float WaitRemaining = 0.0f;
		float RetryRemaining = 0.0f;
		bool bWaiting = false;
		bool bHasDestination = false;
	};
	bool ChooseDestination(AEnemyAIController* Controller, AEnemyCharacter* Enemy, FPatrolMemory& Memory) const;
};
