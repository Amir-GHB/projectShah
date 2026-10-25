// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbTestCharacter.h"

#include "climb/ClimbRuntimeComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCoreTypes.h"

AClimbTestCharacter::AClimbTestCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    ClimbRuntimeComponent = CreateDefaultSubobject<UClimbRuntimeComponent>(
        TEXT("ClimbRuntimeComponent"));
}

void AClimbTestCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (!PlayerInputComponent)
    {
        return;
    }

    PlayerInputComponent->BindKey(EKeys::E, IE_Pressed,
        this, &AClimbTestCharacter::ToggleClimbing);

    PlayerInputComponent->BindKey(EKeys::W, IE_Pressed,
        this, &AClimbTestCharacter::MoveUpPressed);
    PlayerInputComponent->BindKey(EKeys::W, IE_Released,
        this, &AClimbTestCharacter::MoveUpReleased);

    PlayerInputComponent->BindKey(EKeys::S, IE_Pressed,
        this, &AClimbTestCharacter::MoveDownPressed);
    PlayerInputComponent->BindKey(EKeys::S, IE_Released,
        this, &AClimbTestCharacter::MoveDownReleased);

    PlayerInputComponent->BindKey(EKeys::A, IE_Pressed,
        this, &AClimbTestCharacter::MoveLeftPressed);
    PlayerInputComponent->BindKey(EKeys::A, IE_Released,
        this, &AClimbTestCharacter::MoveLeftReleased);

    PlayerInputComponent->BindKey(EKeys::D, IE_Pressed,
        this, &AClimbTestCharacter::MoveRightPressed);
    PlayerInputComponent->BindKey(EKeys::D, IE_Released,
        this, &AClimbTestCharacter::MoveRightReleased);
}

void AClimbTestCharacter::ToggleClimbing()
{
    if (!ClimbRuntimeComponent)
    {
        return;
    }

    if (ClimbRuntimeComponent->IsClimbing())
    {
        ClimbRuntimeComponent->StopClimbing();
        bMoveUp = false;
        bMoveDown = false;
        bMoveLeft = false;
        bMoveRight = false;
        UpdateClimbInput();
        return;
    }

    ClimbRuntimeComponent->StartClimbing();
}

void AClimbTestCharacter::MoveUpPressed()
{
    bMoveUp = true;
    UpdateClimbInput();
}

void AClimbTestCharacter::MoveUpReleased()
{
    bMoveUp = false;
    UpdateClimbInput();
}

void AClimbTestCharacter::MoveDownPressed()
{
    bMoveDown = true;
    UpdateClimbInput();
}

void AClimbTestCharacter::MoveDownReleased()
{
    bMoveDown = false;
    UpdateClimbInput();
}

void AClimbTestCharacter::MoveLeftPressed()
{
    bMoveLeft = true;
    UpdateClimbInput();
}

void AClimbTestCharacter::MoveLeftReleased()
{
    bMoveLeft = false;
    UpdateClimbInput();
}

void AClimbTestCharacter::MoveRightPressed()
{
    bMoveRight = true;
    UpdateClimbInput();
}

void AClimbTestCharacter::MoveRightReleased()
{
    bMoveRight = false;
    UpdateClimbInput();
}

void AClimbTestCharacter::UpdateClimbInput()
{
    if (!ClimbRuntimeComponent)
    {
        return;
    }

    const float Horizontal = (bMoveRight ? 1.0f : 0.0f)
        - (bMoveLeft ? 1.0f : 0.0f);
    const float Vertical = (bMoveUp ? 1.0f : 0.0f)
        - (bMoveDown ? 1.0f : 0.0f);

    ClimbRuntimeComponent->SetClimbInput(
        FVector2D(Horizontal, Vertical).GetClampedToMaxSize(1.0f));
}