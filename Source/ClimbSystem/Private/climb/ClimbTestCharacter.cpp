// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbTestCharacter.h"

#include "Camera/CameraComponent.h"
#include "climb/ClimbRuntimeComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

AClimbTestCharacter::AClimbTestCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Standard third-person movement: the character turns toward travel direction.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->bOrientRotationToMovement = true;
        Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
        Movement->MaxWalkSpeed = 500.0f;
        Movement->BrakingDecelerationWalking = 1800.0f;
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 350.0f;
    CameraBoom->SocketOffset = FVector(0.0f, 55.0f, 65.0f);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 10.0f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 12.0f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

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

    // These conventional axis names work when the project has the usual mouse mappings.
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
}

void AClimbTestCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (ClimbRuntimeComponent && ClimbRuntimeComponent->IsClimbing())
    {
        UpdateClimbInput();
    }
    else
    {
        UpdateGroundMovement();
    }
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
    if (!ClimbRuntimeComponent || !ClimbRuntimeComponent->IsClimbing())
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

void AClimbTestCharacter::UpdateGroundMovement()
{
    if (!Controller)
    {
        return;
    }

    const float ForwardInput = (bMoveUp ? 1.0f : 0.0f)
        - (bMoveDown ? 1.0f : 0.0f);
    const float RightInput = (bMoveRight ? 1.0f : 0.0f)
        - (bMoveLeft ? 1.0f : 0.0f);

    const FRotator ControlRotation = Controller->GetControlRotation();
    const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
    const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDirection, ForwardInput);
    AddMovementInput(RightDirection, RightInput);
}