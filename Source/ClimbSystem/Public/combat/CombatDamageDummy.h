// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatDamageDummy.generated.h"

class UDamageHandlerComponent;
class UStaticMeshComponent;
class USceneComponent;
class UWidgetComponent;
class UCombatHealthBarWidget;

/** Simple damage test target using the same DamageHandlerComponent as combat characters. */
UCLASS(Blueprintable)
class CLIMBSYSTEM_API ACombatDamageDummy : public AActor
{
	GENERATED_BODY()

public:
	ACombatDamageDummy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DummyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDamageHandlerComponent> DamageHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Dummy|UI")
	TSubclassOf<UCombatHealthBarWidget> HealthBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Dummy|UI", meta = (ClampMin = "0.0"))
	FVector HealthBarOffset = FVector(0.0f, 0.0f, 150.0f);

protected:
	virtual void BeginPlay() override;
};
