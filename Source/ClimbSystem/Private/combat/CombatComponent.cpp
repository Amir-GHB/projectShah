// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "combat/WeaponBase.h"
#include "combat/Weapons/Weapon_Bow.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningCharacter = Cast<ACharacter>(GetOwner());
	if (!OwningCharacter.IsValid() || !GetWorld()) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwningCharacter.Get();
	SpawnParams.Instigator = OwningCharacter.Get();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const auto& Pair : WeaponClasses)
	{
		if (!Pair.Value) continue;
		AWeaponBase* Spawned = GetWorld()->SpawnActor<AWeaponBase>(Pair.Value, SpawnParams);
		if (Spawned)
		{
			SpawnedWeapons.Add(Pair.Key, Spawned);
			Spawned->AttachToHolster(OwningCharacter.Get());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CombatComponent] Failed to spawn weapon type %d"), static_cast<int32>(Pair.Key));
		}
	}

	// Intentionally no global AnimInstance montage delegates. All callbacks are bound to
	// the montage playback initiated here and validated with a generation token.
	EquipWeapon(DefaultWeaponType);
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AWeapon_Bow* Bow = Cast<AWeapon_Bow>(CurrentWeapon)) Bow->CancelDraw();
	StopActiveAttack(true, true, 0.0f);
	if (bWeaponSwapActive && OwningCharacter.IsValid() && OwningCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Stop(0.0f);
		}
	}
	++WeaponSwapToken;
	bWeaponSwapActive = false;
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::EquipWeapon(EWeaponType NewWeaponType)
{
	if (!OwningCharacter.IsValid()) return;

	// A weapon change interrupts an in-progress bow draw without releasing a shot.
	if (CurrentCombatState == ECombatState::DrawingBow)
	{
		if (AWeapon_Bow* DrawingBow = Cast<AWeapon_Bow>(CurrentWeapon)) DrawingBow->CancelDraw();
		SetCombatState(ECombatState::Idle);
	}

	if (CurrentCombatState == ECombatState::Attacking || CurrentCombatState == ECombatState::ComboWindow ||
		CurrentCombatState == ECombatState::Recovering)
	{
		if (!bQueueWeaponSwitchDuringAttack)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[CombatComponent] Switch ignored while attack is active."));
			return;
		}
		if (IsValid(GetWeapon(NewWeaponType)))
		{
			QueuedWeaponSwitch = NewWeaponType;
		}
		return;
	}

	if (bWeaponSwapActive)
	{
		// Latest request wins, but do not interrupt the current animation transaction mid-socket swap.
		QueuedWeaponSwitch = NewWeaponType;
		return;
	}

	if (NewWeaponType == CurrentWeaponType && IsValid(CurrentWeapon)) return;
	if (!IsValid(GetWeapon(NewWeaponType)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Requested weapon type %d is not spawned."), static_cast<int32>(NewWeaponType));
		return;
	}

	BeginWeaponSwap(NewWeaponType);
}

void UCombatComponent::BeginWeaponSwap(EWeaponType NewWeaponType)
{
	AWeaponBase* Target = GetWeapon(NewWeaponType);
	if (!IsValid(Target) || !OwningCharacter.IsValid()) return;

	PendingWeaponType = NewWeaponType;
	WeaponBeingUnequipped = CurrentWeapon;
	WeaponBeingEquipped = Target;
	bWeaponSwapActive = true;
	bHolsterPointReached = false;
	bDrawPointReached = false;
	bPlayingUnequipMontage = false;
	++WeaponSwapToken;
	SetCombatState(ECombatState::SwappingWeapon);

	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->CancelTrace();
	}
	PlayUnequipMontageOrContinue();
}

void UCombatComponent::PlayUnequipMontageOrContinue()
{
	AWeaponBase* OldWeapon = WeaponBeingUnequipped.Get();
	UAnimMontage* Montage = IsValid(OldWeapon) ? OldWeapon->GetConfig().UnequipMontage : nullptr;
	UAnimInstance* Anim = OwningCharacter.IsValid() && OwningCharacter->GetMesh() ? OwningCharacter->GetMesh()->GetAnimInstance() : nullptr;

	// Per-weapon fast-switch option: skip only the outgoing weapon's unequip montage.
	if (!IsValid(OldWeapon) || !OldWeapon->bPlayUnequipMontageOnSwitch || !Montage || !Anim)
	{
		if (IsValid(OldWeapon) && OwningCharacter.IsValid())
		{
			OldWeapon->AttachToHolster(OwningCharacter.Get());
		}
		bHolsterPointReached = true;
		PlayEquipMontageOrFinish();
		return;
	}

	bPlayingUnequipMontage = true;
	RefreshAnimationState();
	const uint64 Token = WeaponSwapToken;
	const float Duration = Anim->Montage_Play(Montage);
	if (Duration <= 0.0f)
	{
		OldWeapon->AttachToHolster(OwningCharacter.Get());
		bHolsterPointReached = true;
		PlayEquipMontageOrFinish();
		return;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UCombatComponent::OnWeaponSwapMontageEnded, Token, true);
	Anim->Montage_SetEndDelegate(EndDelegate, Montage);
}

void UCombatComponent::PlayEquipMontageOrFinish()
{
	AWeaponBase* Target = WeaponBeingEquipped.Get();
	UAnimMontage* Montage = IsValid(Target) ? Target->GetConfig().EquipMontage : nullptr;
	UAnimInstance* Anim = OwningCharacter.IsValid() && OwningCharacter->GetMesh() ? OwningCharacter->GetMesh()->GetAnimInstance() : nullptr;

	if (!IsValid(Target) || !Montage || !Anim)
	{
		if (IsValid(Target) && OwningCharacter.IsValid())
		{
			Target->AttachToHand(OwningCharacter.Get());
		}
		bDrawPointReached = true;
		FinishWeaponSwap();
		return;
	}

	bPlayingUnequipMontage = false;
	RefreshAnimationState();
	const uint64 Token = WeaponSwapToken;
	const float Duration = Anim->Montage_Play(Montage);
	if (Duration <= 0.0f)
	{
		Target->AttachToHand(OwningCharacter.Get());
		bDrawPointReached = true;
		FinishWeaponSwap();
		return;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UCombatComponent::OnWeaponSwapMontageEnded, Token, false);
	Anim->Montage_SetEndDelegate(EndDelegate, Montage);
}

void UCombatComponent::NotifyWeaponSwapPoint(EWeaponSwapNotifyAction Action, const UAnimSequenceBase* Animation)
{
	if (!bWeaponSwapActive || !Animation || !OwningCharacter.IsValid()) return;
	AWeaponBase* OldWeapon = WeaponBeingUnequipped.Get();
	AWeaponBase* Target = WeaponBeingEquipped.Get();

	if (Action == EWeaponSwapNotifyAction::HolsterCurrent && bPlayingUnequipMontage &&
		IsValid(OldWeapon) && OldWeapon->GetConfig().UnequipMontage.Get() == Animation)
	{
		OldWeapon->AttachToHolster(OwningCharacter.Get());
		bHolsterPointReached = true;
	}
	else if (Action == EWeaponSwapNotifyAction::DrawTarget && !bPlayingUnequipMontage &&
		IsValid(Target) && Target->GetConfig().EquipMontage.Get() == Animation)
	{
		Target->AttachToHand(OwningCharacter.Get());
		bDrawPointReached = true;
	}
}

void UCombatComponent::OnWeaponSwapMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 SwapToken, bool bWasUnequip)
{
	if (!bWeaponSwapActive || SwapToken != WeaponSwapToken || !OwningCharacter.IsValid()) return;

	if (bWasUnequip)
	{
		AWeaponBase* OldWeapon = WeaponBeingUnequipped.Get();
		if (!bHolsterPointReached && IsValid(OldWeapon))
		{
			OldWeapon->AttachToHolster(OwningCharacter.Get());
			bHolsterPointReached = true;
		}
		PlayEquipMontageOrFinish();
		return;
	}

	AWeaponBase* Target = WeaponBeingEquipped.Get();
	if (!bDrawPointReached && IsValid(Target))
	{
		Target->AttachToHand(OwningCharacter.Get());
		bDrawPointReached = true;
	}
	FinishWeaponSwap();
}

void UCombatComponent::FinishWeaponSwap()
{
	if (!bWeaponSwapActive) return;
	AWeaponBase* Target = WeaponBeingEquipped.Get();
	if (!IsValid(Target))
	{
		// Never leave the state machine locked if a weapon is destroyed during the animation.
		bWeaponSwapActive = false;
		WeaponBeingEquipped.Reset();
		WeaponBeingUnequipped.Reset();
		SetCombatState(ECombatState::Idle);
		return;
	}

	CurrentWeapon = Target;
	CurrentWeaponType = PendingWeaponType;
	CurrentWeapon->AttachToHand(OwningCharacter.Get());
	bWeaponSwapActive = false;
	bPlayingUnequipMontage = false;
	WeaponBeingEquipped.Reset();
	WeaponBeingUnequipped.Reset();
	ClearInputBuffer();
	OnWeaponSwitched.Broadcast(CurrentWeaponType, CurrentWeapon);
	SetCombatState(ECombatState::Idle);

	if (QueuedWeaponSwitch.IsSet())
	{
		const EWeaponType QueuedType = QueuedWeaponSwitch.GetValue();
		QueuedWeaponSwitch.Reset();
		if (QueuedType != CurrentWeaponType) EquipWeapon(QueuedType);
	}
}

void UCombatComponent::SwitchToNextWeapon()
{
	TArray<EWeaponType> ValidTypes;
	for (EWeaponType Type : WeaponCycleOrder)
	{
		if (IsValid(GetWeapon(Type))) ValidTypes.Add(Type);
	}
	if (ValidTypes.IsEmpty()) return;

	const int32 Index = ValidTypes.IndexOfByKey(CurrentWeaponType);
	EquipWeapon(ValidTypes[Index == INDEX_NONE ? 0 : (Index + 1) % ValidTypes.Num()]);
}

AWeaponBase* UCombatComponent::GetWeapon(EWeaponType Type) const
{
	if (const TObjectPtr<AWeaponBase>* Found = SpawnedWeapons.Find(Type)) return Found->Get();
	return nullptr;
}

void UCombatComponent::HandleAttackInput()
{
	if (!OwningCharacter.IsValid() || !IsValid(CurrentWeapon) || bWeaponSwapActive) return;

	// Ranged weapons use Started/Completed input events so holding and releasing are distinct.
	if (AWeapon_Bow* Bow = Cast<AWeapon_Bow>(CurrentWeapon))
	{
		if (CurrentCombatState == ECombatState::Idle || CurrentCombatState == ECombatState::Recovering)
		{
			if (Bow->BeginDraw()) SetCombatState(ECombatState::DrawingBow);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	if (CurrentCombatState == ECombatState::Idle || CurrentCombatState == ECombatState::Recovering)
	{
		LastAttackInputTimestamp = World->GetTimeSeconds();
		bHasBufferedAttackInput = true;
		StartNewCombo();
		return;
	}

	if (CurrentCombatState != ECombatState::Attacking && CurrentCombatState != ECombatState::ComboWindow) return;
	if (bTransitionCommittedForWindow) return;

	LastAttackInputTimestamp = World->GetTimeSeconds();
	bHasBufferedAttackInput = true;
	if (bComboWindowOpen) TryQueueComboTransition();
}


void UCombatComponent::HandleAttackReleased()
{
	if (CurrentCombatState != ECombatState::DrawingBow) return;
	if (AWeapon_Bow* Bow = Cast<AWeapon_Bow>(CurrentWeapon))
	{
		Bow->ReleaseShot();
	}
	SetCombatState(ECombatState::Idle);
}

void UCombatComponent::CancelAttackInput()
{
	if (CurrentCombatState != ECombatState::DrawingBow) return;
	if (AWeapon_Bow* Bow = Cast<AWeapon_Bow>(CurrentWeapon))
	{
		Bow->CancelDraw();
	}
	SetCombatState(ECombatState::Idle);
}

bool UCombatComponent::StartNewCombo()
{
	if (!IsValid(CurrentWeapon) || !OwningCharacter.IsValid() || !OwningCharacter->GetMesh()) return false;
	const FWeaponConfig& Config = CurrentWeapon->GetConfig();
	StopActiveAttack(true, true, 0.1f);
	ActiveAttackWeapon = CurrentWeapon;
	ActiveAttackIndex = 0;
	PendingAttackIndex = INDEX_NONE;
	ActiveSectionIndex = INDEX_NONE;
	PendingSectionIndex = INDEX_NONE;
	bComboWindowOpen = false;
	bTransitionCommittedForWindow = false;
	ClearInputBuffer();

	if (Config.AttackMontageMode == EAttackMontageMode::IndividualAttackMontages)
	{
		if (Config.IndividualAttackMontages.IsEmpty() || !IsValid(Config.IndividualAttackMontages[0]))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Individual montage mode requires at least one valid attack montage."));
			ActiveAttackWeapon.Reset();
			return false;
		}
		return PlayIndividualAttack(0);
	}

	UAnimMontage* Montage = Config.AttackComboMontage;
	if (!IsValid(Montage) || Montage->CompositeSections.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Single-montage mode requires a montage with sections."));
		ActiveAttackWeapon.Reset();
		return false;
	}

	UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance();
	if (!Anim) return false;
	ActiveAttackMontage = Montage;
	ActiveAttackToken = ++CurrentAttackToken;
	TotalSectionsInMontage = Montage->CompositeSections.Num();
	ActiveSectionIndex = 0;
	ActiveAttackIndex = 0;
	const FName FirstSection = Montage->GetSectionName(0);
	const float Duration = Anim->Montage_Play(Montage);
	if (Duration <= 0.0f)
	{
		StopActiveAttack(true, false);
		return false;
	}

	const uint64 Token = ActiveAttackToken;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UCombatComponent::OnMontageInstanceEnded, Token);
	Anim->Montage_SetEndDelegate(EndDelegate, Montage);
	FOnMontageBlendingOutStarted BlendDelegate;
	BlendDelegate.BindUObject(this, &UCombatComponent::OnMontageInstanceBlendingOut, Token);
	Anim->Montage_SetBlendingOutDelegate(BlendDelegate, Montage);
	Anim->Montage_JumpToSection(FirstSection, Montage);
	Anim->Montage_SetNextSection(FirstSection, NAME_None, Montage);
	SetCombatState(ECombatState::Attacking);
	OnComboSectionChanged.Broadcast(0, FirstSection);
	return true;
}

bool UCombatComponent::PlayIndividualAttack(int32 AttackIndex)
{
	if (!OwningCharacter.IsValid() || !OwningCharacter->GetMesh() || !ActiveAttackWeapon.IsValid()) return false;
	const TArray<TObjectPtr<UAnimMontage>>& Montages = ActiveAttackWeapon->GetConfig().IndividualAttackMontages;
	if (!Montages.IsValidIndex(AttackIndex) || !IsValid(Montages[AttackIndex]))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] No valid montage at individual attack index %d."), AttackIndex);
		StopActiveAttack(true, false);
		return false;
	}
	UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance();
	if (!Anim) return false;

	ActiveAttackIndex = AttackIndex;
	PendingAttackIndex = INDEX_NONE;
	ActiveAttackMontage = Montages[AttackIndex];
	ActiveSectionIndex = AttackIndex;
	TotalSectionsInMontage = Montages.Num();
	PendingSectionIndex = INDEX_NONE;
	bComboWindowOpen = false;
	bTransitionCommittedForWindow = false;
	ActiveAttackToken = ++CurrentAttackToken;

	const float Duration = Anim->Montage_Play(ActiveAttackMontage);
	if (Duration <= 0.0f)
	{
		StopActiveAttack(true, false);
		return false;
	}
	const uint64 Token = ActiveAttackToken;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UCombatComponent::OnMontageInstanceEnded, Token);
	Anim->Montage_SetEndDelegate(EndDelegate, ActiveAttackMontage);
	FOnMontageBlendingOutStarted BlendDelegate;
	BlendDelegate.BindUObject(this, &UCombatComponent::OnMontageInstanceBlendingOut, Token);
	Anim->Montage_SetBlendingOutDelegate(BlendDelegate, ActiveAttackMontage);
	SetCombatState(ECombatState::Attacking);
	OnComboSectionChanged.Broadcast(AttackIndex, FName(*FString::Printf(TEXT("Attack_%02d"), AttackIndex + 1)));
	return true;
}

bool UCombatComponent::TryQueueComboTransition()
{
	if (ActiveAttackToken == 0 || !bComboWindowOpen || bTransitionCommittedForWindow) return false;
	if (!ActiveAttackWeapon.IsValid()) return false;

	const FWeaponConfig& Config = ActiveAttackWeapon->GetConfig();
	if (Config.AttackMontageMode == EAttackMontageMode::IndividualAttackMontages)
	{
		const int32 NextIndex = ActiveAttackIndex + 1;
		if (!Config.IndividualAttackMontages.IsValidIndex(NextIndex) || !IsValid(Config.IndividualAttackMontages[NextIndex])) return false;
		PendingAttackIndex = NextIndex;
		bTransitionCommittedForWindow = true;
		ClearInputBuffer();
		return true;
	}
	return TryQueueSectionTransition();
}

bool UCombatComponent::TryQueueSectionTransition()
{
	if (bTransitionCommittedForWindow || !ActiveAttackMontage || ActiveSectionIndex == INDEX_NONE) return false;
	const int32 NextIndex = ActiveSectionIndex + 1;
	if (NextIndex >= TotalSectionsInMontage) return false;
	UAnimInstance* Anim = OwningCharacter.IsValid() && OwningCharacter->GetMesh() ? OwningCharacter->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim) return false;

	const FName CurrentName = ActiveAttackMontage->GetSectionName(ActiveSectionIndex);
	const FName NextName = ActiveAttackMontage->GetSectionName(NextIndex);
	Anim->Montage_SetNextSection(CurrentName, NextName, ActiveAttackMontage);
	Anim->Montage_SetNextSection(NextName, NAME_None, ActiveAttackMontage);
	PendingSectionIndex = NextIndex;
	bTransitionCommittedForWindow = true;
	ClearInputBuffer();
	return true;
}

void UCombatComponent::CommitSectionTransition(int32 NewSectionIndex, FName NewSectionName)
{
	ActiveSectionIndex = NewSectionIndex;
	PendingSectionIndex = INDEX_NONE;
	bTransitionCommittedForWindow = false;
	OnComboSectionChanged.Broadcast(NewSectionIndex, NewSectionName);
}

void UCombatComponent::NotifySectionEntered(FName SectionName, const UAnimSequenceBase* Animation)
{
	if (!ActiveAttackWeapon.IsValid() || ActiveAttackWeapon->GetConfig().AttackMontageMode != EAttackMontageMode::SingleComboMontage ||
		Animation != ActiveAttackMontage || ActiveAttackToken == 0 || !ActiveAttackMontage) return;
	const int32 Index = ActiveAttackMontage->GetSectionIndex(SectionName);
	if (Index != INDEX_NONE && (Index == PendingSectionIndex || (PendingSectionIndex == INDEX_NONE && Index == ActiveSectionIndex + 1)))
	{
		CommitSectionTransition(Index, SectionName);
	}
}

void UCombatComponent::NotifyComboWindowBegin(const UAnimSequenceBase* Animation)
{
	if (Animation != ActiveAttackMontage || ActiveAttackToken == 0 || !ActiveAttackWeapon.IsValid()) return;
	if (ActiveAttackWeapon->GetConfig().AttackMontageMode == EAttackMontageMode::SingleComboMontage &&
		PendingSectionIndex != INDEX_NONE && OwningCharacter.IsValid() && OwningCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance())
		{
			const FName CurrentName = Anim->Montage_GetCurrentSection(ActiveAttackMontage);
			const int32 CurrentIndex = ActiveAttackMontage->GetSectionIndex(CurrentName);
			if (CurrentIndex == PendingSectionIndex) CommitSectionTransition(CurrentIndex, CurrentName);
		}
	}
	bComboWindowOpen = true;
	bTransitionCommittedForWindow = false;
	SetCombatState(ECombatState::ComboWindow);
	if (bHasBufferedAttackInput && IsInputBufferValid()) TryQueueComboTransition();
	else ClearInputBuffer();
}

void UCombatComponent::NotifyComboWindowEnd(const UAnimSequenceBase* Animation)
{
	if (Animation != ActiveAttackMontage || ActiveAttackToken == 0) return;
	bComboWindowOpen = false;
	if (CurrentCombatState == ECombatState::ComboWindow) SetCombatState(ECombatState::Attacking);
	ClearInputBuffer();

	if (ActiveAttackWeapon.IsValid() && ActiveAttackWeapon->GetConfig().AttackMontageMode == EAttackMontageMode::SingleComboMontage &&
		!bTransitionCommittedForWindow && ActiveAttackMontage && ActiveSectionIndex != INDEX_NONE &&
		OwningCharacter.IsValid() && OwningCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_SetNextSection(ActiveAttackMontage->GetSectionName(ActiveSectionIndex), NAME_None, ActiveAttackMontage);
		}
	}
}

void UCombatComponent::NotifyComboTransitionPoint(const UAnimSequenceBase* Animation)
{
	if (Animation != ActiveAttackMontage || ActiveAttackToken == 0 || !ActiveAttackWeapon.IsValid()) return;

	const FWeaponConfig& Config = ActiveAttackWeapon->GetConfig();
	if (Config.AttackMontageMode != EAttackMontageMode::IndividualAttackMontages ||
		PendingAttackIndex == INDEX_NONE || !Config.IndividualAttackMontages.IsValidIndex(PendingAttackIndex) ||
		!IsValid(Config.IndividualAttackMontages[PendingAttackIndex]))
	{
		return;
	}

	const int32 NextIndex = PendingAttackIndex;
	ActiveAttackWeapon->EndTrace();

	// Invalidate the outgoing playback before Montage_Play interrupts it. Its end/blend
	// delegates then cannot finish or reset the newly started combo step.
	ActiveAttackToken = 0;
	ActiveAttackMontage = nullptr;
	PlayIndividualAttack(NextIndex);
}

void UCombatComponent::NotifyMeleeTraceBegin(const UAnimSequenceBase* Animation)
{
	if (Animation == ActiveAttackMontage && ActiveAttackToken != 0 && ActiveAttackWeapon.IsValid()) ActiveAttackWeapon->BeginTrace();
}
void UCombatComponent::NotifyMeleeTraceTick(const UAnimSequenceBase* Animation, float FrameDeltaTime)
{
	if (Animation == ActiveAttackMontage && ActiveAttackToken != 0 && ActiveAttackWeapon.IsValid()) ActiveAttackWeapon->TickTrace();
}
void UCombatComponent::NotifyMeleeTraceEnd(const UAnimSequenceBase* Animation)
{
	if (Animation == ActiveAttackMontage && ActiveAttackToken != 0 && ActiveAttackWeapon.IsValid()) ActiveAttackWeapon->EndTrace();
}

void UCombatComponent::OnMontageInstanceBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint64 AttackToken)
{
	if (AttackToken != ActiveAttackToken || AttackToken == 0 || Montage != ActiveAttackMontage || bIsStoppingAttack) return;
	if (ActiveAttackWeapon.IsValid()) ActiveAttackWeapon->EndTrace();
	if (PendingAttackIndex == INDEX_NONE) SetCombatState(ECombatState::Recovering);
}

void UCombatComponent::OnMontageInstanceEnded(UAnimMontage* Montage, bool bInterrupted, uint64 AttackToken)
{
	if (AttackToken != ActiveAttackToken || AttackToken == 0 || Montage != ActiveAttackMontage || bIsStoppingAttack) return;

	if (!bInterrupted && ActiveAttackWeapon.IsValid() &&
		ActiveAttackWeapon->GetConfig().AttackMontageMode == EAttackMontageMode::IndividualAttackMontages &&
		PendingAttackIndex != INDEX_NONE)
	{
		const int32 NextIndex = PendingAttackIndex;
		if (ActiveAttackWeapon.IsValid()) ActiveAttackWeapon->EndTrace();
		ActiveAttackToken = 0;
		ActiveAttackMontage = nullptr;
		PlayIndividualAttack(NextIndex);
		return;
	}

	FinishAttack(bInterrupted, false, 0.0f);
	OnAttackEnded.Broadcast();
	if (QueuedWeaponSwitch.IsSet())
	{
		const EWeaponType Target = QueuedWeaponSwitch.GetValue();
		QueuedWeaponSwitch.Reset();
		EquipWeapon(Target);
	}
}

void UCombatComponent::FinishAttack(bool bInterrupted, bool bStopMontage, float BlendOutTime)
{
	StopActiveAttack(bInterrupted, bStopMontage, BlendOutTime);
}

void UCombatComponent::ResetCombo()
{
	QueuedWeaponSwitch.Reset();
	StopActiveAttack(true, true, 0.2f);
}

void UCombatComponent::StopActiveAttack(bool bInterrupted, bool bStopMontage, float BlendOutTime)
{
	if (bIsStoppingAttack) return;
	TGuardValue<bool> Guard(bIsStoppingAttack, true);
	ActiveAttackToken = 0; // Invalidate first: Montage_Stop can invoke delegates synchronously.

	if (ActiveAttackWeapon.IsValid()) ActiveAttackWeapon->CancelTrace();
	UAnimMontage* Montage = ActiveAttackMontage;
	if (bStopMontage && Montage && OwningCharacter.IsValid() && OwningCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance())
		{
			if (Anim->Montage_IsPlaying(Montage)) Anim->Montage_Stop(BlendOutTime, Montage);
		}
	}

	ActiveAttackWeapon.Reset();
	ActiveAttackMontage = nullptr;
	ActiveSectionIndex = INDEX_NONE;
	PendingSectionIndex = INDEX_NONE;
	ActiveAttackIndex = INDEX_NONE;
	PendingAttackIndex = INDEX_NONE;
	TotalSectionsInMontage = 0;
	bComboWindowOpen = false;
	bTransitionCommittedForWindow = false;
	ClearInputBuffer();
	if (!bWeaponSwapActive) SetCombatState(ECombatState::Idle);
}

void UCombatComponent::SetCombatState(ECombatState NewState)
{
	const bool bChanged = CurrentCombatState != NewState;
	if (bChanged)
	{
		CurrentCombatState = NewState;
		OnCombatStateChanged.Broadcast(NewState);
	}
	if (bChanged) RefreshAnimationState();
}

FCombatAnimationState UCombatComponent::GetAnimationState() const
{
	FCombatAnimationState Snapshot = AnimationState;
	if (const AWeapon_Bow* Bow = Cast<AWeapon_Bow>(CurrentWeapon))
	{
		Snapshot.bIsDrawingBow = Bow->bIsDrawing;
		Snapshot.BowDrawAlpha = Bow->GetDrawAlpha();
	}
	else
	{
		Snapshot.bIsDrawingBow = false;
		Snapshot.BowDrawAlpha = 0.0f;
	}
	return Snapshot;
}

void UCombatComponent::SetNonCombatAnimation(bool bEnableNonCombatAnimation)
{
	if (bForceNonCombatAnimation == bEnableNonCombatAnimation) return;
	bForceNonCombatAnimation = bEnableNonCombatAnimation;
	RefreshAnimationState();
}

void UCombatComponent::RefreshAnimationState()
{
	const EWeaponType EffectiveWeaponType = (bWeaponSwapActive && ((bHolsterPointReached && !bPlayingUnequipMontage) || !IsValid(CurrentWeapon))) ? PendingWeaponType : CurrentWeaponType;
	AnimationState.WeaponType = EffectiveWeaponType;
	AnimationState.CombatState = CurrentCombatState;
	AnimationState.bWeaponTransitionActive = bWeaponSwapActive;

	if (bForceNonCombatAnimation || (!IsValid(CurrentWeapon) && !bWeaponSwapActive))
	{
		AnimationState.Style = ECombatAnimationStyle::NonCombat;
	}
	else
	{
		switch (EffectiveWeaponType)
		{
		case EWeaponType::Unarmed: AnimationState.Style = ECombatAnimationStyle::Unarmed; break;
		case EWeaponType::Sword: AnimationState.Style = ECombatAnimationStyle::Sword; break;
		case EWeaponType::BullHeadHammer: AnimationState.Style = ECombatAnimationStyle::Hammer; break;
		case EWeaponType::Spear: AnimationState.Style = ECombatAnimationStyle::Spear; break;
		case EWeaponType::Bow: AnimationState.Style = ECombatAnimationStyle::Bow; break;
		default: AnimationState.Style = ECombatAnimationStyle::Custom; break;
		}
	}

	switch (CurrentCombatState)
	{
	case ECombatState::DrawingBow:
		AnimationState.Action = ECombatAnimationAction::Drawing;
		break;
	case ECombatState::Attacking:
	case ECombatState::ComboWindow:
	case ECombatState::Recovering:
		AnimationState.Action = ECombatAnimationAction::Attacking;
		break;
	case ECombatState::Staggered:
		AnimationState.Action = ECombatAnimationAction::Staggered;
		break;
	case ECombatState::Dead:
		AnimationState.Action = ECombatAnimationAction::Dead;
		break;
	case ECombatState::SwappingWeapon:
		AnimationState.Action = bPlayingUnequipMontage ? ECombatAnimationAction::Unequipping : ECombatAnimationAction::Equipping;
		break;
	default:
		AnimationState.Action = ECombatAnimationAction::None;
		break;
	}

	++AnimationState.Revision;
}
void UCombatComponent::ClearInputBuffer()
{
	bHasBufferedAttackInput = false;
	LastAttackInputTimestamp = -1.0;
}
bool UCombatComponent::IsInputBufferValid() const
{
	if (!bHasBufferedAttackInput) return false;
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastAttackInputTimestamp) <= InputBufferDuration;
}
