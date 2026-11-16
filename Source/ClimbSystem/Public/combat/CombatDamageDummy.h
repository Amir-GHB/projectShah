// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatDamageDummy.generated.h"

class UDamageHandlerComponent;
class UDamageReactionComponent;

/**
 * Character-based training target. Assign a skeletal mesh/AnimBP and reaction
 * montages in a Blueprint child to test the complete damage and reaction flow.
 * Its diagnostic health bar is supplied automatically by DamageHandlerComponent.
 */
UCLASS(Blueprintable)
class CLIMBSYSTEM_API ACombatDamageDummy : public ACharacter
{
	GENERATED_BODY()

public:
	ACombatDamageDummy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Damage")
	TObjectPtr<UDamageHandlerComponent> DamageHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Damage")
	TObjectPtr<UDamageReactionComponent> DamageReactionComponent;

	/** Restores health/poise and allows the dummy to play reactions again. */
	UFUNCTION(BlueprintCallable, Category = "Damage Dummy")
	void ResetDummy();
};
