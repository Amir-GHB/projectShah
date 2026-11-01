// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatDamageDummy.generated.h"

class UDamageHandlerComponent;
class UDamageReactionComponent;
class UWidgetComponent;
class UCombatHealthBarWidget;

/**
 * Character-based training target. Assign a skeletal mesh/AnimBP and reaction
 * montages in a Blueprint child to test the complete damage and reaction flow.
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Dummy|UI")
	TSubclassOf<UCombatHealthBarWidget> HealthBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Dummy|UI", meta = (ClampMin = "0.0"))
	FVector HealthBarOffset = FVector(0.0f, 0.0f, 110.0f);

	/** Restores health/poise and allows the dummy to play reactions again. */
	UFUNCTION(BlueprintCallable, Category = "Damage Dummy")
	void ResetDummy();

protected:
	virtual void BeginPlay() override;
};
