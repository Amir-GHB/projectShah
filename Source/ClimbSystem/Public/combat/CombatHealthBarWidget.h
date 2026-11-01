// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHealthBarWidget.generated.h"

class UDamageHandlerComponent;
class UProgressBar;

/** Base widget for a world-space damage dummy health bar. Create a Widget Blueprint
 * derived from this class and add a ProgressBar named HealthBar. */
UCLASS(Abstract, Blueprintable)
class CLIMBSYSTEM_API UCombatHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	void BindToDamageHandler(UDamageHandlerComponent* DamageHandler);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDamageHandlerComponent> BoundDamageHandler = nullptr;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth);
};
