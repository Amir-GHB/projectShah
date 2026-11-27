// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "riding/HorseCharacter.h"
#include "riding/RidingComponent.h"
#include "combat/CombatComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/Controller.h"

AHorseCharacter::AHorseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("HorseCameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("HorseFollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->MaxAcceleration = 900.0f;
		Movement->BrakingDecelerationWalking = 500.0f;
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, TurnRate, 0.0f);
		Movement->MaxWalkSpeedCrouched = 0.0f;
	}
}

void AHorseCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->RotationRate = FRotator(0.0f, TurnRate, 0.0f);
	}
}

void AHorseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Controller)
	{
		TargetControlYaw = Controller->GetControlRotation().Yaw;
	}
}

void AHorseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Enhanced) return;

	if (MoveAction) Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHorseCharacter::OnMove);
	if (LookAction) Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHorseCharacter::OnLook);
	if (SprintAction)
	{
		Enhanced->BindAction(SprintAction, ETriggerEvent::Started, this, &AHorseCharacter::OnSprintStarted);
		Enhanced->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHorseCharacter::OnSprintEnded);
		Enhanced->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AHorseCharacter::OnSprintEnded);
	}
	if (DismountAction) Enhanced->BindAction(DismountAction, ETriggerEvent::Started, this, &AHorseCharacter::OnDismount);
	if (AttackAction)
	{
		Enhanced->BindAction(AttackAction, ETriggerEvent::Started, this, &AHorseCharacter::OnAttackStarted);
		Enhanced->BindAction(AttackAction, ETriggerEvent::Completed, this, &AHorseCharacter::OnAttackReleased);
		Enhanced->BindAction(AttackAction, ETriggerEvent::Canceled, this, &AHorseCharacter::OnAttackReleased);
	}
	if (NextWeaponAction) Enhanced->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &AHorseCharacter::OnNextWeapon);
	if (GuardAction)
	{
		Enhanced->BindAction(GuardAction, ETriggerEvent::Started, this, &AHorseCharacter::OnGuardStarted);
		Enhanced->BindAction(GuardAction, ETriggerEvent::Completed, this, &AHorseCharacter::OnGuardEnded);
		Enhanced->BindAction(GuardAction, ETriggerEvent::Canceled, this, &AHorseCharacter::OnGuardEnded);
	}
}

bool AHorseCharacter::CanAcceptRider() const
{
	return IsValid(this) && !IsValid(CurrentRider) && RidingState == ERidingState::Unmounted;
}

FTransform AHorseCharacter::GetMountWorldTransform(EHorseMountDirection Direction) const
{
	const FTransform Offset = Direction == EHorseMountDirection::Left ? LeftMountOffset
		: Direction == EHorseMountDirection::Right ? RightMountOffset : RearMountOffset;
	return Offset * GetActorTransform();
}

void AHorseCharacter::SetCurrentRider(ACharacter* Rider)
{
	CurrentRider = Rider;
	RiderCombatComponent = IsValid(Rider) ? Rider->FindComponentByClass<UCombatComponent>() : nullptr;
}

void AHorseCharacter::SetRidingState(ERidingState NewState)
{
	RidingState = NewState;
}

void AHorseCharacter::SetSprinting(bool bEnable)
{
	bIsSprinting = bEnable;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = bEnable ? SprintSpeed : WalkSpeed;
	}
}

float AHorseCharacter::GetCurrentHorseSpeed() const
{
	return GetVelocity().Size2D();
}

void AHorseCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (!Controller || RidingState != ERidingState::Mounted) return;

	CurrentMoveInput = Input.Y;
	CurrentTurnInput = Input.X;

	const FRotator ControlYaw(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::Y);

	if (Input.Y >= 0.0f)
	{
		AddMovementInput(Forward, Input.Y);
	}
	else if (bAllowReverseMovement)
	{
		AddMovementInput(-GetActorForwardVector(), FMath::Abs(Input.Y) * (ReverseSpeed / FMath::Max(1.0f, WalkSpeed)));
	}
	if (bAllowStrafeMovement && !FMath::IsNearlyZero(Input.X))
	{
		AddMovementInput(Right, Input.X * 0.5f);
	}
}

void AHorseCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void AHorseCharacter::OnSprintStarted(const FInputActionValue& Value) { SetSprinting(true); }
void AHorseCharacter::OnSprintEnded(const FInputActionValue& Value) { SetSprinting(false); }

void AHorseCharacter::OnDismount(const FInputActionValue& Value)
{
	if (ACharacter* Rider = CurrentRider.Get())
	{
		if (URidingComponent* Riding = Rider->FindComponentByClass<URidingComponent>())
		{
			Riding->RequestDismount();
		}
	}
}

void AHorseCharacter::OnAttackStarted(const FInputActionValue& Value)
{
	if (UCombatComponent* Combat = RiderCombatComponent.Get()) Combat->HandleAttackInput();
}

void AHorseCharacter::OnAttackReleased(const FInputActionValue& Value)
{
	if (UCombatComponent* Combat = RiderCombatComponent.Get())
	{
		Combat->HandleAttackReleased();
		Combat->CancelAttackInput();
	}
}

void AHorseCharacter::OnNextWeapon(const FInputActionValue& Value)
{
	if (UCombatComponent* Combat = RiderCombatComponent.Get()) Combat->SwitchToNextWeapon();
}

void AHorseCharacter::OnGuardStarted(const FInputActionValue& Value)
{
	if (UCombatComponent* Combat = RiderCombatComponent.Get()) Combat->SetGuardMode(true);
}

void AHorseCharacter::OnGuardEnded(const FInputActionValue& Value)
{
	if (UCombatComponent* Combat = RiderCombatComponent.Get()) Combat->SetGuardMode(false);
}
