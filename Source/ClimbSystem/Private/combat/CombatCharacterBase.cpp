// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatCharacterBase.h"
#include "combat/CombatComponent.h"
#include "combat/CombatTargetingComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/DamageReactionComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/DamageEvents.h"

ACombatCharacterBase::ACombatCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	DamageHandlerComponent = CreateDefaultSubobject<UDamageHandlerComponent>(TEXT("DamageHandlerComponent"));
	CombatTargetingComponent = CreateDefaultSubobject<UCombatTargetingComponent>(TEXT("CombatTargetingComponent"));
	DamageReactionComponent = CreateDefaultSubobject<UDamageReactionComponent>(TEXT("DamageReactionComponent"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 320.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->JumpZVelocity = 600.0f;
		Movement->AirControl = 0.25f;
	}
}

void ACombatCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				if (DefaultMappingContext)
				{
					Subsystem->AddMappingContext(DefaultMappingContext, 0);
				}
			}
		}
	}
}

void ACombatCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACombatCharacterBase::OnMove);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACombatCharacterBase::OnLook);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACombatCharacterBase::OnJumpEnded);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ACombatCharacterBase::OnJumpEnded);
		}
		if (AttackAction)
		{
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnAttackTriggered);
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Completed, this, &ACombatCharacterBase::OnAttackReleased);
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Canceled, this, &ACombatCharacterBase::OnAttackCanceled);
		}
		if (NextWeaponAction)
		{
			EnhancedInput->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnNextWeaponTriggered);
		}
		if (TargetLockAction)
		{
			EnhancedInput->BindAction(TargetLockAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnTargetLockTriggered);
		}
	}
}

void ACombatCharacterBase::OnMove(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (!Controller || Input.IsNearlyZero()) return;

	const FRotator ControlRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::Y), Input.X);
}

void ACombatCharacterBase::OnLook(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void ACombatCharacterBase::OnJumpStarted(const FInputActionValue& Value)
{
	Jump();
}

void ACombatCharacterBase::OnJumpEnded(const FInputActionValue& Value)
{
	StopJumping();
}

void ACombatCharacterBase::OnAttackTriggered(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->HandleAttackInput();
	}
}


void ACombatCharacterBase::OnAttackReleased(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->HandleAttackReleased();
	}
}

void ACombatCharacterBase::OnAttackCanceled(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->CancelAttackInput();
	}
}

void ACombatCharacterBase::OnNextWeaponTriggered(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->SwitchToNextWeapon();
	}
}

void ACombatCharacterBase::OnTargetLockTriggered(const FInputActionValue& Value)
{
	if (CombatTargetingComponent)
	{
		CombatTargetingComponent->ToggleManualLock();
	}
}

float ACombatCharacterBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (DamageHandlerComponent)
	{
		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage = ActualDamage;
		DamageInfo.DamageMultiplier = 1.0f;
		DamageInfo.DamageCauser = DamageCauser;
		if (IsValid(DamageCauser))
		{
			DamageInfo.DamageDirection = (GetActorLocation()
				- DamageCauser->GetActorLocation()).GetSafeNormal();
		}
		DamageInfo.Normalize();

		FHitResult HitResult;
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			HitResult = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo;
		}
		DamageHandlerComponent->ApplyDamage(DamageCauser, this, DamageInfo, HitResult);
	}

	return ActualDamage;
}
