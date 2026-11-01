// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "combat/DamageHandlerComponent.h"

void UCombatHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The base widget works on its own; a Widget Blueprint can replace/style this bar.
	if (!HealthBar && WidgetTree)
	{
		HealthBar = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("HealthBar"));
		WidgetTree->RootWidget = HealthBar;
	}
}

void UCombatHealthBarWidget::BindToDamageHandler(UDamageHandlerComponent* DamageHandler)
{
	if (BoundDamageHandler)
	{
		BoundDamageHandler->OnHealthChanged.RemoveDynamic(
			this, &UCombatHealthBarWidget::HandleHealthChanged);
	}

	BoundDamageHandler = DamageHandler;
	if (!BoundDamageHandler)
	{
		if (HealthBar) HealthBar->SetPercent(0.0f);
		return;
	}

	BoundDamageHandler->OnHealthChanged.AddDynamic(
		this, &UCombatHealthBarWidget::HandleHealthChanged);
	HandleHealthChanged(
		BoundDamageHandler->CurrentHealth,
		BoundDamageHandler->MaxHealth);
}

void UCombatHealthBarWidget::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(MaxHealth > 0.0f
			? FMath::Clamp(NewHealth / MaxHealth, 0.0f, 1.0f) : 0.0f);
	}
}

void UCombatHealthBarWidget::NativeDestruct()
{
	if (BoundDamageHandler)
	{
		BoundDamageHandler->OnHealthChanged.RemoveDynamic(
			this, &UCombatHealthBarWidget::HandleHealthChanged);
		BoundDamageHandler = nullptr;
	}

	Super::NativeDestruct();
}
