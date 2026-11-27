// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "riding/HorseCharacter.h"
#include "riding/RidingComponent.h"
#include "combat/CombatComponent.h"
#include "combat/CombatCharacterBase.h"
#include "combat/CombatTypes.h"
#include "combat/CombatTargetingComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

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
		Movement->bOrientRotationToMovement = false;
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
		Enhanced->BindAction(AttackAction, ETriggerEvent::Canceled, this, &AHorseCharacter::OnAttackCanceled);
	}
	if (NextWeaponAction) Enhanced->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &AHorseCharacter::OnNextWeapon);
	if (ParryAction) Enhanced->BindAction(ParryAction, ETriggerEvent::Started, this, &AHorseCharacter::OnParry);
	if (TargetLockAction) Enhanced->BindAction(TargetLockAction, ETriggerEvent::Started, this, &AHorseCharacter::OnTargetLock);
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

FTransform AHorseCharacter::GetDismountWorldTransform(EHorseMountDirection Direction) const
{
	const FVector Offset = Direction == EHorseMountDirection::Left ? LeftDismountOffset
		: Direction == EHorseMountDirection::Center ? RearDismountOffset : RightDismountOffset;
	const FVector WorldLocation = GetActorLocation()
		+ GetActorRightVector() * Offset.Y
		+ GetActorForwardVector() * Offset.X
		+ FVector::UpVector * Offset.Z;
	return FTransform(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), WorldLocation);
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

void AHorseCharacter::SetRiderInputActions(UInputAction* Move, UInputAction* Look, UInputAction* Sprint,
	UInputAction* Dismount, UInputAction* Attack, UInputAction* NextWeapon,
	UInputAction* Guard, UInputAction* Parry, UInputAction* TargetLock)
{
	MoveAction = Move;
	LookAction = Look;
	SprintAction = Sprint;
	DismountAction = Dismount;
	AttackAction = Attack;
	NextWeaponAction = NextWeapon;
	GuardAction = Guard;
	ParryAction = Parry;
	TargetLockAction = TargetLock;
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

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	if (Input.Y < -0.05f)
	{
		if (bAllowReverseMovement)
		{
			if (Movement) Movement->MaxWalkSpeed = ReverseSpeed;
			AddMovementInput(GetActorForwardVector(), Input.Y);
		}
		else if (Movement)
		{
			Movement->MaxWalkSpeed = WalkSpeed;
		}
	}
	else
	{
		if (Movement) Movement->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
		const float CurrentYaw = GetActorRotation().Yaw;
		const float DesiredYaw = Controller->GetControlRotation().Yaw;
		const float NewYaw = FMath::FixedTurn(CurrentYaw, DesiredYaw, TurnRate * DeltaSeconds);
		SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
		AddMovementInput(GetActorForwardVector(), Input.Y);
	}

	if (bAllowStrafeMovement && !FMath::IsNearlyZero(Input.X))
	{
		AddMovementInput(GetActorRightVector(), Input.X * 0.5f);
	}
	else if (!FMath::IsNearlyZero(Input.X))
	{
		AddActorWorldRotation(FRotator(0.0f, Input.X * TurnRate * DeltaSeconds, 0.0f));
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
	UCombatComponent* Combat = RiderCombatComponent.Get();
	ACombatCharacterBase* Rider = Cast<ACombatCharacterBase>(CurrentRider.Get());
	if (!Combat) return;
	if (Combat->CurrentWeaponType == EWeaponType::Rope
		&& (Combat->CurrentCombatState == ECombatState::Idle || Combat->CurrentCombatState == ECombatState::Recovering)
		&& Combat->GetAnimationState().DefenseState == ECombatDefenseState::None)
	{
		bMountedRopeAimInputHeld = true;
		if (Rider) Rider->BeginAimCameraMode();
		return;
	}
	Combat->HandleAttackInput();
}

void AHorseCharacter::OnAttackReleased(const FInputActionValue& Value)
{
	UCombatComponent* Combat = RiderCombatComponent.Get();
	if (!Combat) return;
	if (bMountedRopeAimInputHeld)
	{
		bMountedRopeAimInputHeld = false;
		Combat->HandleAttackInput();
		if (Combat->CurrentCombatState != ECombatState::Attacking && Combat->CurrentCombatState != ECombatState::ComboWindow)
		{
			if (ACombatCharacterBase* Rider = Cast<ACombatCharacterBase>(CurrentRider.Get())) Rider->EndAimCameraMode();
		}
		return;
	}
	Combat->HandleAttackReleased();
	Combat->CancelAttackInput();
}

void AHorseCharacter::OnAttackCanceled(const FInputActionValue& Value)
{
	if (bMountedRopeAimInputHeld)
	{
		bMountedRopeAimInputHeld = false;
		if (ACombatCharacterBase* Rider = Cast<ACombatCharacterBase>(CurrentRider.Get())) Rider->EndAimCameraMode();
	}
	if (UCombatComponent* Combat = RiderCombatComponent.Get()) Combat->CancelAttackInput();
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

void AHorseCharacter::OnParry(const FInputActionValue& Value)
{
	if (UCombatComponent* Combat = RiderCombatComponent.Get()) Combat->BeginParryAttempt();
}

void AHorseCharacter::OnTargetLock(const FInputActionValue& Value)
{
	if (ACharacter* Rider = CurrentRider.Get())
	{
		if (UCombatTargetingComponent* Targeting = Rider->FindComponentByClass<UCombatTargetingComponent>())
			Targeting->ToggleManualLock();
	}
}
