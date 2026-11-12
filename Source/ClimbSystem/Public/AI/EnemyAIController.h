// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class AEnemyCharacter;
class UDamageHandlerComponent;

UCLASS()
class CLIMBSYSTEM_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()
public:
	AEnemyAIController();
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }
	void ClearCurrentTarget();
	FVector GetInvestigationLocation() const { return InvestigationLocation; }
	bool HasActiveInvestigation() const;
	int32 GetPatrolPointIndex() const { return PatrolPointIndex; }
	void AdvancePatrolPointIndex(int32 PointCount);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy AI")
	TObjectPtr<UAIPerceptionComponent> AIPerception;
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

private:
	TWeakObjectPtr<AEnemyCharacter> ControlledEnemy;
	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<UDamageHandlerComponent> CurrentTargetDamageHandler;
	FVector InvestigationLocation = FVector::ZeroVector;
	double InvestigationExpiryTime = 0.0;
	int32 PatrolPointIndex = 0;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	UFUNCTION()
	void HandleComboWindowOpened();
	UFUNCTION()
	void HandleCurrentTargetDeath(AActor* KillerActor);
	bool IsValidCombatTarget(const AActor* Candidate) const;
	void SetCurrentTarget(AActor* Target);
	void SetBlackboardTarget(AActor* Target);
	void SetBlackboardInvestigation(const FVector& Location);
	void ClearBlackboardInvestigation();
};
