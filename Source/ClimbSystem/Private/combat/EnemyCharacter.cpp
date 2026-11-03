// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/EnemyCharacter.h"

#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/DamageReactionComponent.h"
#include "AI/EnemyAIController.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	DamageHandlerComponent = CreateDefaultSubobject<UDamageHandlerComponent>(TEXT("DamageHandlerComponent"));
	DamageReactionComponent = CreateDefaultSubobject<UDamageReactionComponent>(TEXT("DamageReactionComponent"));

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->MaxWalkSpeed = 300.0f;
		Movement->NavAgentProps.bCanCrouch = true;
	}
}

float AEnemyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!DamageHandlerComponent || DamageHandlerComponent->bIsDead || ActualDamage <= 0.0f)
	{
		return ActualDamage;
	}

	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage = ActualDamage;
	DamageInfo.DamageMultiplier = 1.0f;
	DamageInfo.DamageCauser = DamageCauser;
	if (IsValid(DamageCauser))
	{
		DamageInfo.DamageDirection =
			(GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
	}
	DamageInfo.Normalize();

	FHitResult HitResult;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		HitResult = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo;
	}
	DamageHandlerComponent->ApplyDamage(DamageCauser, this, DamageInfo, HitResult);
	return ActualDamage;
}
