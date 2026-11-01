// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatDamageDummy.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "combat/CombatHealthBarWidget.h"
#include "combat/DamageHandlerComponent.h"

ACombatDamageDummy::ACombatDamageDummy()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DummyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DummyMesh"));
	DummyMesh->SetupAttachment(SceneRoot);
	DummyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DummyMesh->SetCollisionObjectType(ECC_WorldDynamic);
	DummyMesh->SetCollisionResponseToAllChannels(ECR_Block);
	DummyMesh->SetGenerateOverlapEvents(false);
	DummyMesh->SetSimulatePhysics(false);

	DamageHandlerComponent = CreateDefaultSubobject<UDamageHandlerComponent>(TEXT("DamageHandlerComponent"));

	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarComponent"));
	HealthBarComponent->SetupAttachment(SceneRoot);
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::World);
	HealthBarComponent->SetDrawSize(FVector2D(120.0f, 16.0f));
	HealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarComponent->SetRelativeLocation(HealthBarOffset);

	HealthBarWidgetClass = UCombatHealthBarWidget::StaticClass();
}

void ACombatDamageDummy::BeginPlay()
{
	Super::BeginPlay();

	if (!HealthBarComponent)
	{
		return;
	}

	HealthBarComponent->SetRelativeLocation(HealthBarOffset);
	if (HealthBarWidgetClass)
	{
		HealthBarComponent->SetWidgetClass(HealthBarWidgetClass);
		HealthBarComponent->InitWidget();

		if (UCombatHealthBarWidget* HealthWidget =
			Cast<UCombatHealthBarWidget>(HealthBarComponent->GetUserWidgetObject()))
		{
			HealthWidget->BindToDamageHandler(DamageHandlerComponent);
		}
	}
}
