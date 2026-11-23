// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatCharacterBase.h"
#include "combat/CombatComponent.h"
#include "combat/CombatTargetingComponent.h"
#include "combat/CombatPositioningComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/DamageReactionComponent.h"
#include "combat/ThrowableTreeActor.h"
#include "MotionWarpingComponent.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/DamageEvents.h"

ACombatCharacterBase::ACombatCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	DamageHandlerComponent = CreateDefaultSubobject<UDamageHandlerComponent>(TEXT("DamageHandlerComponent"));
	CombatTargetingComponent = CreateDefaultSubobject<UCombatTargetingComponent>(TEXT("CombatTargetingComponent"));
	CombatPositioningComponent = CreateDefaultSubobject<UCombatPositioningComponent>(TEXT("CombatPositioningComponent"));
	DamageReactionComponent = CreateDefaultSubobject<UDamageReactionComponent>(TEXT("DamageReactionComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

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

	if (CameraBoom)
	{
		DefaultCameraArmLength = CameraBoom->TargetArmLength;
		DefaultCameraSocketOffset = CameraBoom->SocketOffset;
	}

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

void ACombatCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAimCameraTransitionActive || !CameraBoom)
	{
		return;
	}

	const FVector TargetOffset = bAimCameraModeActive ? AimCameraSocketOffset : DefaultCameraSocketOffset;
	const float TargetArmLength = bAimCameraModeActive ? AimCameraArmLength : DefaultCameraArmLength;
	CameraBoom->SocketOffset = FMath::VInterpTo(
		CameraBoom->SocketOffset, TargetOffset, DeltaSeconds, FMath::Max(0.1f, AimCameraInterpSpeed));
	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, FMath::Max(0.1f, AimCameraInterpSpeed));

	if (CameraBoom->SocketOffset.Equals(TargetOffset, 0.1f)
		&& FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetArmLength, 0.1f))
	{
		CameraBoom->SocketOffset = TargetOffset;
		CameraBoom->TargetArmLength = TargetArmLength;
		bAimCameraTransitionActive = false;
	}
}

void ACombatCharacterBase::BeginAimCameraMode()
{
	if (bAimCameraModeActive)
	{
		return;
	}

	bAimCameraModeActive = true;
	bAimCameraTransitionActive = true;

	if (!AimCrosshairWidget && AimCrosshairWidgetClass)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			AimCrosshairWidget = CreateWidget<UUserWidget>(PlayerController, AimCrosshairWidgetClass);
		}
	}
	if (AimCrosshairWidget && !AimCrosshairWidget->IsInViewport())
	{
		AimCrosshairWidget->AddToViewport(100);
	}
}

void ACombatCharacterBase::EndAimCameraMode()
{
	bRopeAimInputHeld = false;
	if (!bAimCameraModeActive && !AimCrosshairWidget)
	{
		return;
	}

	bAimCameraModeActive = false;
	bAimCameraTransitionActive = CameraBoom != nullptr;
	if (AimCrosshairWidget)
	{
		AimCrosshairWidget->RemoveFromParent();
		AimCrosshairWidget = nullptr;
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
		if (GuardAction)
		{
			EnhancedInput->BindAction(GuardAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnGuardStarted);
			EnhancedInput->BindAction(GuardAction, ETriggerEvent::Completed, this, &ACombatCharacterBase::OnGuardEnded);
			EnhancedInput->BindAction(GuardAction, ETriggerEvent::Canceled, this, &ACombatCharacterBase::OnGuardEnded);
		}
		if (ParryAction)
		{
			EnhancedInput->BindAction(ParryAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnParryTriggered);
		}
		if (NextWeaponAction)
		{
			EnhancedInput->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnNextWeaponTriggered);
		}
		if (TargetLockAction)
		{
			EnhancedInput->BindAction(TargetLockAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnTargetLockTriggered);
		}
		if (ThrowNearbyActorAction)
		{
			EnhancedInput->BindAction(ThrowNearbyActorAction, ETriggerEvent::Started, this, &ACombatCharacterBase::OnThrowNearbyActorTriggered);
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
	if (CombatComponent && (CombatComponent->CurrentCombatState == ECombatState::Finishing
		|| CombatComponent->CurrentCombatState == ECombatState::Dead)) return;
	Jump();
}

void ACombatCharacterBase::OnJumpEnded(const FInputActionValue& Value)
{
	StopJumping();
}

void ACombatCharacterBase::OnAttackTriggered(const FInputActionValue& Value)
{
	if (!CombatComponent)
	{
		return;
	}

	if (CombatComponent->CurrentWeaponType == EWeaponType::Rope
		&& (CombatComponent->CurrentCombatState == ECombatState::Idle
			|| CombatComponent->CurrentCombatState == ECombatState::Recovering)
		&& CombatComponent->GetAnimationState().DefenseState == ECombatDefenseState::None)
	{
		bRopeAimInputHeld = true;
		BeginAimCameraMode();
		return;
	}

	CombatComponent->HandleAttackInput();
}


void ACombatCharacterBase::OnAttackReleased(const FInputActionValue& Value)
{
	if (bRopeAimInputHeld)
	{
		bRopeAimInputHeld = false;
		if (CombatComponent)
		{
			CombatComponent->HandleAttackInput();
			if (CombatComponent->CurrentCombatState != ECombatState::Attacking
				&& CombatComponent->CurrentCombatState != ECombatState::ComboWindow)
			{
				EndAimCameraMode();
			}
		}
		else
		{
			EndAimCameraMode();
		}
		return;
	}

	if (CombatComponent)
	{
		CombatComponent->HandleAttackReleased();
	}
}

void ACombatCharacterBase::OnAttackCanceled(const FInputActionValue& Value)
{
	if (bRopeAimInputHeld)
	{
		bRopeAimInputHeld = false;
		EndAimCameraMode();
	}
	if (CombatComponent)
	{
		CombatComponent->CancelAttackInput();
	}
}

void ACombatCharacterBase::OnGuardStarted(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->SetGuardMode(true);
	}
}

void ACombatCharacterBase::OnGuardEnded(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->SetGuardMode(false);
	}
}

void ACombatCharacterBase::OnParryTriggered(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->BeginParryAttempt();
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

void ACombatCharacterBase::OnThrowNearbyActorTriggered(const FInputActionValue& Value)
{
	if (!GetWorld() || (CombatComponent && (CombatComponent->CurrentCombatState == ECombatState::Dead
		|| CombatComponent->CurrentCombatState == ECombatState::Finishing)))
	{
		return;
	}

	AThrowableTreeActor* NearbyTree = nullptr;
	float BestDistanceSquared = FMath::Square(300.0f);
	for (TActorIterator<AThrowableTreeActor> It(GetWorld()); It; ++It)
	{
		AThrowableTreeActor* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->CanBeThrown())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			NearbyTree = Candidate;
		}
	}

	if (IsValid(NearbyTree))
	{
		NearbyTree->ThrowAtClosestVisibleEnemy(this);
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
