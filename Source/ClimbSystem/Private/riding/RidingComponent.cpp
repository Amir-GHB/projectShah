// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "riding/RidingComponent.h"
#include "riding/HorseCharacter.h"
#include "AI/EnemyAIController.h"
#include "combat/EnemyCharacter.h"
#include "combat/CombatCharacterBase.h"
#include "combat/CombatComponent.h"
#include "combat/CombatAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"

URidingComponent::URidingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URidingComponent::BeginPlay()
{
	Super::BeginPlay();
	Rider = Cast<ACombatCharacterBase>(GetOwner());
}

void URidingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsHorseHiding) ResetHorseHiding();
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CameraInputRestoreTimer);
	if (bTransitionInProgress)
	{
		if (ACombatCharacterBase* Character = Rider.Get())
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				Movement->SetMovementMode(MOVE_Walking);
			Character->GetCapsuleComponent()->SetCollisionEnabled(PreviousCapsuleCollisionMode);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void URidingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bTransitionInProgress) return;

	ACombatCharacterBase* Character = Rider.Get();
	if (!IsValid(Character))
	{
		SetComponentTickEnabled(false);
		bTransitionInProgress = false;
		return;
	}

	TransitionElapsed += DeltaTime;
	const float Duration = FMath::Max(0.05f, MountTransitionDuration);
	const float Alpha = FMath::Clamp(TransitionElapsed / Duration, 0.0f, 1.0f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	Character->SetActorLocationAndRotation(
		FMath::Lerp(TransitionStart.GetLocation(), TransitionTarget.GetLocation(), SmoothAlpha),
		FQuat::Slerp(TransitionStart.GetRotation(), TransitionTarget.GetRotation(), SmoothAlpha),
		false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		bTransitionInProgress = false;
		SetComponentTickEnabled(false);
		if (RidingState == ERidingState::Mounting) FinishMount();
		else if (RidingState == ERidingState::Dismounting) FinishDismount();
	}
}

bool URidingComponent::TryMountNearbyHorse()
{
	ACombatCharacterBase* Character = Rider.Get();
	if (!IsValid(Character) || RidingState != ERidingState::Unmounted || !GetWorld()) return false;

	AHorseCharacter* BestHorse = nullptr;
	float BestDistance = FMath::Square(MountSearchRadius);
	for (TActorIterator<AHorseCharacter> It(GetWorld()); It; ++It)
	{
		AHorseCharacter* Horse = *It;
		if (!IsValid(Horse) || !Horse->CanAcceptRider()) continue;
		const float Distance = FVector::DistSquared(Character->GetActorLocation(), Horse->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestHorse = Horse;
		}
	}
	if (!BestHorse) return false;

	const FVector ToRider = Character->GetActorLocation() - BestHorse->GetActorLocation();
	const float Side = FVector::DotProduct(ToRider, BestHorse->GetActorRightVector());
	const bool bBehind = FVector::DotProduct(ToRider, BestHorse->GetActorForwardVector()) < -0.35f;
	const EHorseMountDirection Direction = bBehind ? EHorseMountDirection::Center
		: (Side < 0.0f ? EHorseMountDirection::Left : EHorseMountDirection::Right);
	return TryMountHorse(BestHorse, Direction);
}

bool URidingComponent::TryMountHorse(AHorseCharacter* Horse, EHorseMountDirection Direction)
{
	ACombatCharacterBase* Character = Rider.Get();
	if (!IsValid(Character) || !IsValid(Horse) || !Horse->CanAcceptRider()
		|| RidingState != ERidingState::Unmounted || Direction == EHorseMountDirection::None)
	{
		return false;
	}
	if (Character->CombatComponent
		&& Character->CombatComponent->CurrentCombatState != ECombatState::Idle
		&& Character->CombatComponent->CurrentCombatState != ECombatState::Recovering)
	{
		return false;
	}
	if (Character->CombatComponent
		&& Character->CombatComponent->GetAnimationState().DefenseState != ECombatDefenseState::None)
	{
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return false;

	CurrentHorse = Horse;
	CachedPlayerController = PC;
	GetWorld()->GetTimerManager().ClearTimer(CameraInputRestoreTimer);
	CameraTransitionHorse.Reset();
	Character->ConfigureHorseInput(Horse);
	Horse->SetCurrentRider(Character);
	Horse->SetRidingState(ERidingState::Mounting);
	Character->EndAimCameraMode();
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		PreviousCapsuleCollisionMode = Capsule->GetCollisionEnabled();
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Character->CameraBoom)
	{
		bWasCameraCollisionTestEnabled = Character->CameraBoom->bDoCollisionTest;
		Character->CameraBoom->bDoCollisionTest = false;
	}
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);

	const FTransform Start = Character->GetActorTransform();
	const FTransform Target = Horse->GetMountWorldTransform(Direction);
	BeginTransformTransition(Target, ERidingState::Mounting, Direction);
	TransitionStart = Start;
	PlayTransitionMontage(true, Direction);
	return true;
}

void URidingComponent::RequestDismount()
{
	ACombatCharacterBase* Character = Rider.Get();
	AHorseCharacter* Horse = CurrentHorse;
	if (!IsValid(Character) || !IsValid(Horse) || RidingState != ERidingState::Mounted) return;
	if (Character->IsAimCameraModeActive()) return;
	if (Character->CombatComponent
		&& Character->CombatComponent->CurrentCombatState != ECombatState::Idle
		&& Character->CombatComponent->CurrentCombatState != ECombatState::Recovering) return;
	if (Character->CombatComponent
		&& Character->CombatComponent->GetAnimationState().DefenseState != ECombatDefenseState::None) return;

	if (bIsHorseHiding) ResetHorseHiding();
	const FTransform DismountTransform = Horse->GetDismountWorldTransform(PreferredDismountDirection);
	DismountTargetLocation = DismountTransform.GetLocation();
	DismountTargetRotation = DismountTransform.Rotator();

	if (APlayerController* PC = CachedPlayerController.Get())
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		Movement->DisableMovement();

	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	BeginTransformTransition(FTransform(DismountTargetRotation, DismountTargetLocation), ERidingState::Dismounting, PreferredDismountDirection);
	PlayTransitionMontage(false, PreferredDismountDirection);
	Horse->SetRidingState(ERidingState::Dismounting);
}

bool URidingComponent::StartHorseHiding()
{
	if (RidingState != ERidingState::Mounted || !IsValid(CurrentHorse)
		|| IsInRidingTransition() || HorseHidingState != EHorseHidingState::NotHiding)
	{
		return false;
	}

	ACombatCharacterBase* Character = Rider.Get();
	if (!IsValid(Character) || Character->IsAimCameraModeActive()) return false;
	if (Character->CombatComponent
		&& Character->CombatComponent->CurrentCombatState != ECombatState::Idle
		&& Character->CombatComponent->CurrentCombatState != ECombatState::Recovering)
	{
		return false;
	}

	bHideInputHeld = true;
	bHideReleaseRequested = false;
	bIsHorseHiding = true;
	HorseHidingState = EHorseHidingState::Entering;
	NotifyHorseHiddenForAI(true);
	UpdateRiderAnimationState();

	UAnimMontage* HideMontage = HorseHideSide == EHorseHideSide::Right && IsValid(HideRightMontage)
		? HideRightMontage : HideLeftMontage;
	UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsValid(HideMontage) || !Anim || Anim->Montage_Play(HideMontage) <= 0.0f)
	{
		HorseHidingState = EHorseHidingState::Hidden;
		UpdateRiderAnimationState();
		return true;
	}

	FOnMontageEnded HideMontageEndedDelegate =
		FOnMontageEnded::CreateUObject(this, &URidingComponent::HandleHideStartMontageEnded);
	Anim->Montage_SetEndDelegate(HideMontageEndedDelegate, HideMontage);
	return true;
}

void URidingComponent::StopHorseHiding()
{
	bHideInputHeld = false;
	if (HorseHidingState == EHorseHidingState::Entering)
	{
		bHideReleaseRequested = true;
	}
	else if (HorseHidingState == EHorseHidingState::Hidden)
	{
		BeginHorseHideExit();
	}
}

bool URidingComponent::ToggleHorseHiding()
{
	if (HorseHidingState == EHorseHidingState::NotHiding) return StartHorseHiding();
	StopHorseHiding();
	return true;
}

void URidingComponent::SetHorseHiding(bool bHide)
{
	if (bHide) StartHorseHiding();
	else ResetHorseHiding();
}

void URidingComponent::BeginHorseHideExit()
{
	if (HorseHidingState == EHorseHidingState::Entering)
	{
		bHideReleaseRequested = true;
		return;
	}
	if (HorseHidingState != EHorseHidingState::Hidden) return;

	ACombatCharacterBase* Character = Rider.Get();
	if (!IsValid(Character))
	{
		ResetHorseHiding();
		return;
	}

	HorseHidingState = EHorseHidingState::Exiting;
	bHideReleaseRequested = false;
	UpdateRiderAnimationState();

	UAnimMontage* UnhideMontage = HorseHideSide == EHorseHideSide::Right && IsValid(UnhideRightMontage)
		? UnhideRightMontage : UnhideLeftMontage;
	UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsValid(UnhideMontage) || !Anim || Anim->Montage_Play(UnhideMontage) <= 0.0f)
	{
		FinishHorseHideExit();
		return;
	}

	FOnMontageEnded UnhideMontageEndedDelegate =
		FOnMontageEnded::CreateUObject(this, &URidingComponent::HandleHideEndMontageEnded);
	Anim->Montage_SetEndDelegate(UnhideMontageEndedDelegate, UnhideMontage);
}

void URidingComponent::HandleHideStartMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (HorseHidingState != EHorseHidingState::Entering) return;

	HorseHidingState = EHorseHidingState::Hidden;
	UpdateRiderAnimationState();
	if (bHideReleaseRequested || !bHideInputHeld) BeginHorseHideExit();
}

void URidingComponent::HandleHideEndMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (HorseHidingState == EHorseHidingState::Exiting) FinishHorseHideExit();
}

void URidingComponent::FinishHorseHideExit()
{
	if (HorseHidingState == EHorseHidingState::NotHiding) return;
	bIsHorseHiding = false;
	bHideInputHeld = false;
	bHideReleaseRequested = false;
	HorseHidingState = EHorseHidingState::NotHiding;
	NotifyHorseHiddenForAI(false);
	UpdateRiderAnimationState();
}

void URidingComponent::NotifyHorseHiddenForAI(bool bHidden)
{
	AHorseCharacter* Horse = CurrentHorse;
	if (!IsValid(Horse)) return;

	// AI perceives the possessed horse pawn, not the attached rider.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
		{
			if (AEnemyAIController* AI = Cast<AEnemyAIController>(It->GetController()))
			{
				AI->HandlePlayerHidden(Horse, bHidden);
			}
		}
	}
}

void URidingComponent::ResetHorseHiding()
{
	const bool bWasHidden = bIsHorseHiding;

	// Clear the state before stopping montages so their interrupted callbacks
	// cannot advance the hiding state machine during a forced transition.
	bIsHorseHiding = false;
	bHideInputHeld = false;
	bHideReleaseRequested = false;
	HorseHidingState = EHorseHidingState::NotHiding;

	ACombatCharacterBase* Character = Rider.Get();
	UAnimInstance* Anim = IsValid(Character) && Character->GetMesh()
		? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim)
	{
		UAnimMontage* HideMontage = HorseHideSide == EHorseHideSide::Right && IsValid(HideRightMontage)
			? HideRightMontage : HideLeftMontage;
		UAnimMontage* UnhideMontage = HorseHideSide == EHorseHideSide::Right && IsValid(UnhideRightMontage)
			? UnhideRightMontage : UnhideLeftMontage;
		if (IsValid(HideMontage)) Anim->Montage_Stop(0.1f, HideMontage);
		if (IsValid(UnhideMontage)) Anim->Montage_Stop(0.1f, UnhideMontage);
	}

	if (bWasHidden) NotifyHorseHiddenForAI(false);
	UpdateRiderAnimationState();
}

void URidingComponent::HandleRiderDeath()
{
	ACombatCharacterBase* Character = Rider.Get();
	AHorseCharacter* Horse = CurrentHorse;
	if (!IsValid(Character) || !IsValid(Horse) || RidingState != ERidingState::Mounted) return;

	if (bIsHorseHiding) SetHorseHiding(false);
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		Capsule->SetCollisionEnabled(PreviousCapsuleCollisionMode);
	if (Character->CameraBoom)
		Character->CameraBoom->bDoCollisionTest = bWasCameraCollisionTestEnabled;

	Horse->SetSprinting(false);
	Horse->SetCurrentRider(nullptr);
	Horse->SetRidingState(ERidingState::Unmounted);
	if (UCharacterMovementComponent* HorseMovement = Horse->GetCharacterMovement())
	{
		HorseMovement->StopMovementImmediately();
	}
	CameraTransitionHorse = Horse;
	if (Horse->FollowCamera) Horse->FollowCamera->Activate(true);
	if (Character->FollowCamera) Character->FollowCamera->Activate(true);

	RidingState = ERidingState::Falling;
	MountDirection = EHorseMountDirection::None;
	CurrentHorse = nullptr;
	if (APlayerController* PC = CachedPlayerController.Get())
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		PC->Possess(Character);
		PC->SetViewTarget(Horse);
		PC->SetViewTargetWithBlend(Character, CameraBlendDuration);
		SchedulePlayerInputRestore();
	}
	UpdateRiderAnimationState();
}

void URidingComponent::BeginTransformTransition(const FTransform& Target, ERidingState NewState, EHorseMountDirection Direction)
{
	ACombatCharacterBase* Character = Rider.Get();
	if (!IsValid(Character)) return;

	RidingState = NewState;
	MountDirection = Direction;
	TransitionStart = Character->GetActorTransform();
	TransitionTarget = Target;
	TransitionElapsed = 0.0f;
	bTransitionInProgress = true;
	SetComponentTickEnabled(true);
	UpdateRiderAnimationState();
}

void URidingComponent::FinishMount()
{
	ACombatCharacterBase* Character = Rider.Get();
	AHorseCharacter* Horse = CurrentHorse;
	if (!IsValid(Character) || !IsValid(Horse)) return;

	if (USkeletalMeshComponent* HorseMesh = Horse->GetMesh())
	{
		if (HorseMesh->DoesSocketExist(Horse->RiderAttachSocketName))
		{
			Character->AttachToComponent(HorseMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale, Horse->RiderAttachSocketName);
		}
		else
		{
			Character->AttachToActor(Horse, FAttachmentTransformRules::KeepWorldTransform);
			UE_LOG(LogTemp, Warning, TEXT("[Riding] Horse '%s' has no rider socket '%s'; attached to actor root."),
				*Horse->GetName(), *Horse->RiderAttachSocketName.ToString());
		}
	}

	RidingState = ERidingState::Mounted;
	Horse->SetRidingState(ERidingState::Mounted);
	UpdateRiderAnimationState();

	CameraTransitionHorse = Horse;
	if (Character->FollowCamera) Character->FollowCamera->Activate(true);
	if (Horse->FollowCamera) Horse->FollowCamera->Activate(true);
	if (APlayerController* PC = CachedPlayerController.Get())
	{
		PC->Possess(Horse);
		PC->SetViewTarget(Character);
		PC->SetViewTargetWithBlend(Horse, CameraBlendDuration);
		SchedulePlayerInputRestore();
	}
}

void URidingComponent::FinishDismount()
{
	ACombatCharacterBase* Character = Rider.Get();
	AHorseCharacter* Horse = CurrentHorse;
	if (!IsValid(Character) || !IsValid(Horse)) return;

	Character->SetActorLocationAndRotation(DismountTargetLocation, DismountTargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		Capsule->SetCollisionEnabled(PreviousCapsuleCollisionMode);
	if (Character->CameraBoom)
		Character->CameraBoom->bDoCollisionTest = bWasCameraCollisionTestEnabled;
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->StopMovementImmediately();
	}

	Horse->SetSprinting(false);
	Horse->SetCurrentRider(nullptr);
	Horse->SetRidingState(ERidingState::Unmounted);
	CameraTransitionHorse = Horse;
	if (Horse->FollowCamera) Horse->FollowCamera->Activate(true);
	if (Character->FollowCamera) Character->FollowCamera->Activate(true);

	if (APlayerController* PC = CachedPlayerController.Get())
	{
		PC->Possess(Character);
		PC->SetViewTarget(Horse);
		PC->SetViewTargetWithBlend(Character, CameraBlendDuration);
		SchedulePlayerInputRestore();
	}
	CurrentHorse = nullptr;
	RidingState = ERidingState::Unmounted;
	MountDirection = EHorseMountDirection::None;
	UpdateRiderAnimationState();
}

void URidingComponent::SchedulePlayerInputRestore()
{
	if (CameraBlendDuration <= 0.0f || !GetWorld())
	{
		RestorePlayerInput();
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(CameraInputRestoreTimer);
	GetWorld()->GetTimerManager().SetTimer(
		CameraInputRestoreTimer, this, &URidingComponent::RestorePlayerInput,
		CameraBlendDuration, false);
}

void URidingComponent::RestorePlayerInput()
{
	if (RidingState == ERidingState::Mounted)
	{
		if (ACombatCharacterBase* Character = Rider.Get())
		{
			if (Character->FollowCamera) Character->FollowCamera->Deactivate();
		}
	}
	else
	{
		if (AHorseCharacter* Horse = CameraTransitionHorse.Get())
		{
			if (Horse->FollowCamera) Horse->FollowCamera->Deactivate();
		}
	}
	CameraTransitionHorse.Reset();

	if (APlayerController* PC = CachedPlayerController.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
}

void URidingComponent::PlayTransitionMontage(bool bMounting, EHorseMountDirection Direction)
{
	ACombatCharacterBase* Character = Rider.Get();
	if (!IsValid(Character) || !Character->GetMesh()) return;
	UAnimMontage* Montage = nullptr;
	if (bMounting)
	{
		Montage = Direction == EHorseMountDirection::Left ? MountLeftMontage
			: Direction == EHorseMountDirection::Right ? MountRightMontage : MountRearMontage;
	}
	else
	{
		Montage = Direction == EHorseMountDirection::Left ? DismountLeftMontage
			: Direction == EHorseMountDirection::Right ? DismountRightMontage : DismountRearMontage;
	}
	if (IsValid(Montage))
	{
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
			Anim->Montage_Play(Montage);
	}
}

void URidingComponent::SetRidingTransition(ERidingState NewState, EHorseMountDirection Direction)
{
	RidingState = NewState;
	MountDirection = Direction;
	UpdateRiderAnimationState();
}

void URidingComponent::UpdateRiderAnimationState()
{
	if (ACombatCharacterBase* Character = Rider.Get())
	{
		if (UCombatAnimInstance* Anim = Cast<UCombatAnimInstance>(Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr))
		{
			Anim->RidingState = RidingState;
			Anim->MountDirection = MountDirection;
			Anim->bIsHorseHiding = bIsHorseHiding && RidingState == ERidingState::Mounted;
			Anim->HorseHidingState = RidingState == ERidingState::Mounted ? HorseHidingState : EHorseHidingState::NotHiding;
			Anim->HorseHideSide = bIsHorseHiding ? HorseHideSide : EHorseHideSide::None;
		}
	}
}
