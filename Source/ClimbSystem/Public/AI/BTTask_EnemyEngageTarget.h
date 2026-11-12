// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_EnemyEngageTarget.generated.h"

UCLASS()
class CLIMBSYSTEM_API UBTTask_EnemyEngageTarget : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_EnemyEngageTarget();
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FName TargetActorKey = TEXT("TargetActor");
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "50.0"))
	float MoveUpdateDistance = 180.0f;
	virtual uint16 GetInstanceMemorySize() const override;
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
private:
	struct FEngageMemory
	{
		FVector LastMoveGoal = FVector::ZeroVector;
		FVector RelocationDestination = FVector::ZeroVector;
		float AttackCooldown = 0.0f;
		bool bHasMoveGoal = false;
		bool bRelocationIssued = false;
	};
};
