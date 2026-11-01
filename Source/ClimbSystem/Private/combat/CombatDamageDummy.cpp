// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatDamageDummy.h"

#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "combat/CombatHealthBarWidget.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/DamageReactionComponent.h"

ACombatDamageDummy::ACombatDamageDummy()
{
	PrimaryActorTick.bCanEverTick = false;

	// ACharacter supplies a Pawn root capsule and a skeletal mesh. The mesh and
	// AnimBP are assigned on a Blueprint child so project-specific assets remain
	// out of native code.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);

	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetGenerateOverlapEvents(false);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = false;
		Movement->DisableMovement();
	}

	DamageHandlerComponent = CreateDefaultSubobject<UDamageHandlerComponent>(TEXT("DamageHandlerComponent"));
	DamageReactionComponent = CreateDefaultSubobject<UDamageReactionComponent>(TEXT("DamageReactionComponent"));

	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarComponent"));
	HealthBarComponent->SetupAttachment(GetRootComponent());
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

void ACombatDamageDummy::ResetDummy()
{
	if (DamageHandlerComponent)
	{
		DamageHandlerComponent->ResetStats();
	}

	if (DamageReactionComponent)
	{
		DamageReactionComponent->ResetReactionState();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
}
