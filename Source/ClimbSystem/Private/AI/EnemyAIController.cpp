// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#include "AI/EnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "combat/EnemyCharacter.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"

namespace EnemyAIKeys
{
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName HasTarget(TEXT("HasTarget"));
	static const FName InvestigationLocation(TEXT("InvestigationLocation"));
	static const FName HasInvestigation(TEXT("HasInvestigation"));
}

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
	AEnemyCharacter* Enemy = ControlledEnemy.Get();
	if (!IsValid(Enemy)) return;
	PatrolPointIndex = 0;
	SightConfig->SightRadius = Enemy->SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(Enemy->LoseSightRadius, Enemy->SightRadius);
	SightConfig->PeripheralVisionAngleDegrees = Enemy->SightPeripheralAngle;
	HearingConfig->HearingRange = Enemy->HearingRange;
	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	AIPerception->RequestStimuliListenerUpdate();
	AIPerception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
	if (Enemy->CombatComponent)
		Enemy->CombatComponent->OnComboWindowOpened.AddUniqueDynamic(this, &AEnemyAIController::HandleComboWindowOpened);
	if (!IsValid(Enemy->BehaviorTreeAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyAI] %s has no BehaviorTreeAsset. Assign a Behavior Tree and Blackboard in the enemy Blueprint."), *GetNameSafe(Enemy));
		return;
	}
	if (!RunBehaviorTree(Enemy->BehaviorTreeAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyAI] Failed to start Behavior Tree '%s' for %s."),
			*GetNameSafe(Enemy->BehaviorTreeAsset), *GetNameSafe(Enemy));
	}
}

void AEnemyAIController::OnUnPossess()
{
	if (BrainComponent) BrainComponent->StopLogic(TEXT("Enemy unpossessed"));
	if (AIPerception)
		AIPerception->OnTargetPerceptionUpdated.RemoveDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
	if (ControlledEnemy.IsValid() && ControlledEnemy->CombatComponent)
	{
		ControlledEnemy->CombatComponent->OnComboWindowOpened.RemoveDynamic(this, &AEnemyAIController::HandleComboWindowOpened);
		ControlledEnemy->CombatComponent->CancelAttackInput();
		if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(ControlledEnemy.Get()))
			Mage->CancelPendingMagicAttack();
	}
	SetCurrentTarget(nullptr);
	ControlledEnemy.Reset();
	Super::OnUnPossess();
}

bool AEnemyAIController::HasActiveInvestigation() const
{
	return GetWorld() && InvestigationExpiryTime > GetWorld()->GetTimeSeconds();
}

void AEnemyAIController::AdvancePatrolPointIndex(int32 PointCount)
{
	PatrolPointIndex = PointCount > 0 ? (PatrolPointIndex + 1) % PointCount : 0;
}

void AEnemyAIController::SetBlackboardTarget(AActor* Target)
{
	if (UBlackboardComponent* Blackboard = GetBlackboardComponent())
	{
		Blackboard->SetValueAsObject(EnemyAIKeys::TargetActor, Target);
		Blackboard->SetValueAsBool(EnemyAIKeys::HasTarget, IsValid(Target));
	}
}

void AEnemyAIController::SetCurrentTarget(AActor* Target)
{
	if (!IsValidCombatTarget(Target))
	{
		Target = nullptr;
	}

	if (CurrentTarget.Get() != Target)
	{
		if (AActor* PreviousTarget = CurrentTarget.Get())
		{
			PreviousTarget->OnDestroyed.RemoveDynamic(this, &AEnemyAIController::HandleCurrentTargetDestroyed);
		}
		if (UDamageHandlerComponent* PreviousHandler = CurrentTargetDamageHandler.Get())
		{
			PreviousHandler->OnDeath.RemoveDynamic(this, &AEnemyAIController::HandleCurrentTargetDeath);
		}
		CurrentTargetDamageHandler.Reset();
		CurrentTarget = Target;

		if (IsValid(Target))
		{
			Target->OnDestroyed.AddUniqueDynamic(this, &AEnemyAIController::HandleCurrentTargetDestroyed);
			if (UDamageHandlerComponent* TargetHandler = Target->FindComponentByClass<UDamageHandlerComponent>())
			{
				CurrentTargetDamageHandler = TargetHandler;
				TargetHandler->OnDeath.AddUniqueDynamic(this, &AEnemyAIController::HandleCurrentTargetDeath);
			}
		}
	}

	SetBlackboardTarget(CurrentTarget.Get());
}

void AEnemyAIController::ClearCurrentTarget()
{
	SetCurrentTarget(nullptr);
	ClearFocus(EAIFocusPriority::Gameplay);
	StopMovement();
}

void AEnemyAIController::HandleCurrentTargetDeath(AActor* KillerActor)
{
	SetCurrentTarget(nullptr);
	InvestigationExpiryTime = 0.0;
	ClearBlackboardInvestigation();
	ClearFocus(EAIFocusPriority::Gameplay);
	StopMovement();

	AEnemyCharacter* Enemy = ControlledEnemy.Get();
	if (!IsValid(Enemy)) return;

	if (Enemy->CombatComponent)
	{
		// Cancel a held bow draw before StopActiveAttack transitions the component to Idle.
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
}

void AEnemyAIController::HandleCurrentTargetDestroyed(AActor* DestroyedActor)
{
	HandleCurrentTargetDeath(DestroyedActor);
}

void AEnemyAIController::SetBlackboardInvestigation(const FVector& Location)
{
	if (UBlackboardComponent* Blackboard = GetBlackboardComponent())
	{
		Blackboard->SetValueAsVector(EnemyAIKeys::InvestigationLocation, Location);
		Blackboard->SetValueAsBool(EnemyAIKeys::HasInvestigation, HasActiveInvestigation());
	}
}

void AEnemyAIController::ClearBlackboardInvestigation()
{
	if (UBlackboardComponent* Blackboard = GetBlackboardComponent())
		Blackboard->SetValueAsBool(EnemyAIKeys::HasInvestigation, false);
}

void AEnemyAIController::HandleComboWindowOpened()
{
	AEnemyCharacter* Enemy = ControlledEnemy.Get();
	AActor* Target = CurrentTarget.Get();
	if (!IsValid(Enemy) || !Enemy->CombatComponent || !IsValidCombatTarget(Target)) return;
	if (FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation()) <= Enemy->AttackRange &&
		Enemy->CombatComponent->CurrentCombatState == ECombatState::ComboWindow)
		Enemy->CombatComponent->HandleAttackInput();
}

void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	AEnemyCharacter* Enemy = ControlledEnemy.Get();
	if (!IsValid(Enemy) || !IsValid(Actor) || Actor == Enemy) return;
	const FAISenseID SightID = UAISense::GetSenseID(UAISense_Sight::StaticClass());
	const FAISenseID HearingID = UAISense::GetSenseID(UAISense_Hearing::StaticClass());

	if (Stimulus.Type == SightID)
	{
		if (Stimulus.WasSuccessfullySensed() && IsValidCombatTarget(Actor))
		{
			if (UCombatComponent* PlayerCombat = Actor->FindComponentByClass<UCombatComponent>())
				PlayerCombat->RegisterEncounterEnemy(Enemy);
			SetCurrentTarget(Actor);
			InvestigationExpiryTime = 0.0;
			SetBlackboardTarget(Actor);
			ClearBlackboardInvestigation();
			SetFocus(Actor);
		}
		else if (CurrentTarget.Get() == Actor)
		{
			InvestigationLocation = Stimulus.StimulusLocation;
			InvestigationExpiryTime = GetWorld() ?
				GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, Enemy->InvestigateDuration) : 0.0;
			SetCurrentTarget(nullptr);
			SetBlackboardInvestigation(InvestigationLocation);
			ClearFocus(EAIFocusPriority::Gameplay);
			if (Enemy->CombatComponent) Enemy->CombatComponent->CancelAttackInput();
			if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Enemy))
				Mage->CancelPendingMagicAttack();
		}
	}
	else if (Stimulus.Type == HearingID && Stimulus.WasSuccessfullySensed() && !CurrentTarget.IsValid())
	{
		InvestigationLocation = Stimulus.StimulusLocation;
		InvestigationExpiryTime = GetWorld() ?
			GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, Enemy->InvestigateDuration) : 0.0;
		SetBlackboardInvestigation(InvestigationLocation);
	}
}

bool AEnemyAIController::IsValidCombatTarget(const AActor* Candidate) const
{
	if (!IsValid(Candidate) || Candidate == GetPawn()) return false;
	const APawn* CandidatePawn = Cast<APawn>(Candidate);
	if (!CandidatePawn || !CandidatePawn->IsPlayerControlled()) return false;
	const UDamageHandlerComponent* DamageHandler = Candidate->FindComponentByClass<UDamageHandlerComponent>();
	return !DamageHandler || !DamageHandler->bIsDead;
}
