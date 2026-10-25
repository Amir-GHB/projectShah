// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatAnimInstance.h"
#include "combat/CombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UCombatAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RefreshCachedReferences();
}

void UCombatAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* PawnOwner = TryGetPawnOwner();
	ACharacter* Character = Cast<ACharacter>(PawnOwner);
	if (CachedCharacter.Get() != Character || !CachedCombatComponent.IsValid())
	{
		RefreshCachedReferences();
	}

	if (UCombatComponent* Combat = CachedCombatComponent.Get())
	{
		const FCombatAnimationState PreviousState = CombatAnimationState;
		const FCombatAnimationState NewState = Combat->GetAnimationState();
		CombatAnimationState = NewState;

		if (LastAnimationStateRevision != NewState.Revision)
		{
			OnCombatAnimationStateChanged(PreviousState, NewState);
			LastAnimationStateRevision = NewState.Revision;
		}
	}
	else
	{
		CombatAnimationState = FCombatAnimationState();
	}

	Character = CachedCharacter.Get();
	if (!IsValid(Character))
	{
		GroundSpeed = 0.0f;
		bIsMoving = false;
		bIsFalling = false;
		bIsAccelerating = false;
		return;
	}

	const FVector Velocity = Character->GetVelocity();
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
	bIsMoving = GroundSpeed > 3.0f;

	if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		bIsFalling = Movement->IsFalling();
		bIsAccelerating = !Movement->GetCurrentAcceleration().IsNearlyZero();
	}
	else
	{
		bIsFalling = false;
		bIsAccelerating = false;
	}
}

UCombatComponent* UCombatAnimInstance::GetCombatComponent() const
{
	return CachedCombatComponent.Get();
}

void UCombatAnimInstance::RefreshCachedReferences()
{
	LastAnimationStateRevision = INDEX_NONE;
	ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	CachedCharacter = Character;
	CachedCombatComponent = IsValid(Character) ? Character->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!CachedCombatComponent.IsValid())
	{
		LastAnimationStateRevision = INDEX_NONE;
	}
}
