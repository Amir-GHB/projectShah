// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "AI/EnemyAIController.h"

#include "AI/EnemyCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Navigation/PathFollowingComponent.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = false;

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerception);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1800.0f;
	SightConfig->LoseSightRadius = 2200.0f;
	SightConfig->PeripheralVisionAngleDegrees = 75.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 1200.0f;
	HearingConfig->LoSHearingRange = 0.0f;
	HearingConfig->bUseLoSHearing = false;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	AIPerception->ConfigureSense(*HearingConfig);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledEnemy = Cast<AEnemyCharacter>(InPawn);
	if (!ControlledEnemy.IsValid())
	{
		return;
	}

	// Character settings are copied into the runtime sense configs here, keeping setup on the enemy BP.
	SightConfig->SightRadius = ControlledEnemy->SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(
		ControlledEnemy->LoseSightRadius, ControlledEnemy->SightRadius);
	SightConfig->PeripheralVisionAngleDegrees = ControlledEnemy->SightPeripheralAngle;
	HearingConfig->HearingRange = ControlledEnemy->HearingRange;
	AIPerception->RequestStimuliListenerUpdate();

	AIPerception->OnTargetPerceptionUpdated.AddUniqueDynamic(
		this, &AEnemyAIController::HandleTargetPerceptionUpdated);

	PatrolPointIndex = 0;
	GetWorldTimerManager().SetTimer(UpdateTimer, this,
		&AEnemyAIController::UpdateAI, 0.25f, true);
	UpdateAI();
}

void AEnemyAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(UpdateTimer);
	GetWorldTimerManager().ClearTimer(PatrolWaitTimer);
	if (AIPerception)
	{
		AIPerception->OnTargetPerceptionUpdated.RemoveDynamic(
			this, &AEnemyAIController::HandleTargetPerceptionUpdated);
	}
	ControlledEnemy.Reset();
	CurrentTarget.Reset();
	Super::OnUnPossess();
}

void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!ControlledEnemy.IsValid() || !IsValid(Actor) || Actor == ControlledEnemy.Get())
	{
		return;
	}

	const FAISenseID SightSenseID = UAISense::GetSenseID(UAISense_Sight::StaticClass());
	const FAISenseID HearingSenseID = UAISense::GetSenseID(UAISense_Hearing::StaticClass());

	if (Stimulus.Type == SightSenseID)
	{
		if (Stimulus.WasSuccessfullySensed() && IsValidCombatTarget(Actor))
		{
			CurrentTarget = Actor;
			InvestigationTimeRemaining = 0.0f;
			SetFocus(Actor);
		}
		else if (CurrentTarget.Get() == Actor)
		{
			// Losing sight leaves a short-lived investigation point, not omniscient tracking.
			InvestigationLocation = Stimulus.StimulusLocation;
			InvestigationTimeRemaining = ControlledEnemy->InvestigateDuration;
			ClearTarget();
		}
	}
	else if (Stimulus.Type == HearingSenseID && Stimulus.WasSuccessfullySensed())
	{
		if (!CurrentTarget.IsValid())
		{
			InvestigationLocation = Stimulus.StimulusLocation;
			InvestigationTimeRemaining = ControlledEnemy->InvestigateDuration;
		}
	}
}

bool AEnemyAIController::IsValidCombatTarget(const AActor* Candidate) const
{
	if (!IsValid(Candidate) || Candidate == GetPawn())
	{
		return false;
	}

	const APawn* CandidatePawn = Cast<APawn>(Candidate);
	if (!CandidatePawn || !CandidatePawn->IsPlayerControlled())
	{
		return false;
	}

	const UDamageHandlerComponent* DamageHandler =
		Candidate->FindComponentByClass<UDamageHandlerComponent>();
	return !DamageHandler || !DamageHandler->bIsDead;
}

void AEnemyAIController::ClearTarget()
{
	CurrentTarget.Reset();
	ClearFocus(EAIFocusPriority::Gameplay);
	StopMovement();
}

void AEnemyAIController::UpdateAI()
{
	AEnemyCharacter* Enemy = ControlledEnemy.Get();
	if (!IsValid(Enemy) || !Enemy->GetCharacterMovement())
	{
		return;
	}

	if (Enemy->DamageHandlerComponent && Enemy->DamageHandlerComponent->bIsDead)
	{
		ClearTarget();
		StopMovement();
		GetWorldTimerManager().ClearTimer(UpdateTimer);
		return;
	}

	AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - 0.25f);

	AActor* Target = CurrentTarget.Get();
	if (IsValidCombatTarget(Target))
	{
		SetFocus(Target);
		const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());

		if (Distance > Enemy->AttackRange)
		{
			MoveToActor(Target, Enemy->AttackRange * 0.8f, true, true, true);
		}
		else
		{
			StopMovement();
			if (Enemy->CombatComponent
				&& Enemy->CombatComponent->CurrentCombatState == ECombatState::Idle
				&& AttackCooldownRemaining <= 0.0f)
			{
				Enemy->CombatComponent->HandleAttackInput();
				AttackCooldownRemaining = Enemy->AttackRetryInterval;
			}
		}
		return;
	}

	if (CurrentTarget.IsValid())
	{
		ClearTarget();
	}

	if (InvestigationTimeRemaining > 0.0f)
	{
		InvestigationTimeRemaining = FMath::Max(0.0f, InvestigationTimeRemaining - 0.25f);
		SetFocalPoint(InvestigationLocation);
		if (FVector::Dist2D(Enemy->GetActorLocation(), InvestigationLocation) > Enemy->PatrolAcceptanceRadius)
		{
			MoveToLocation(InvestigationLocation, Enemy->PatrolAcceptanceRadius);
		}
		else
		{
			StopMovement();
		}
		return;
	}

	ClearFocus(EAIFocusPriority::Gameplay);
	UpdatePatrol();
}

void AEnemyAIController::UpdatePatrol()
{
	AEnemyCharacter* Enemy = ControlledEnemy.Get();
	if (!IsValid(Enemy) || Enemy->PatrolPoints.IsEmpty())
	{
		StopMovement();
		return;
	}

	if (bWaitingAtPatrolPoint)
	{
		return;
	}

	AActor* PatrolPoint = Enemy->PatrolPoints.IsValidIndex(PatrolPointIndex)
		? Enemy->PatrolPoints[PatrolPointIndex].Get() : nullptr;

	if (!IsValid(PatrolPoint))
	{
		MoveToNextPatrolPoint();
		return;
	}

	if (FVector::Dist2D(Enemy->GetActorLocation(), PatrolPoint->GetActorLocation())
		<= Enemy->PatrolAcceptanceRadius)
	{
		StopMovement();
		bWaitingAtPatrolPoint = true;
		GetWorldTimerManager().SetTimer(PatrolWaitTimer, this,
			&AEnemyAIController::FinishPatrolWait, Enemy->PatrolWaitTime, false);
		return;
	}

	MoveToActor(PatrolPoint, Enemy->PatrolAcceptanceRadius, true, true, true);
}

void AEnemyAIController::FinishPatrolWait()
{
	bWaitingAtPatrolPoint = false;
	MoveToNextPatrolPoint();
}

void AEnemyAIController::MoveToNextPatrolPoint()
{
	AEnemyCharacter* Enemy = ControlledEnemy.Get();
	if (!IsValid(Enemy) || Enemy->PatrolPoints.IsEmpty())
	{
		return;
	}

	PatrolPointIndex = (PatrolPointIndex + 1) % Enemy->PatrolPoints.Num();
	AActor* NextPoint = Enemy->PatrolPoints.IsValidIndex(PatrolPointIndex)
		? Enemy->PatrolPoints[PatrolPointIndex].Get() : nullptr;
	if (IsValid(NextPoint))
	{
		MoveToActor(NextPoint, Enemy->PatrolAcceptanceRadius, true, true, true);
	}
}
