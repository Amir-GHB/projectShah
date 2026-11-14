// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/CombatComponent.h"
#include "combat/CombatTargetingComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "combat/WeaponBase.h"
#include "combat/DamageReactionComponent.h"
#include "combat/AnimNotifyState_ParryWindow.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "combat/Weapons/Weapon_Bow.h"
#include "combat/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "BrainComponent.h"

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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardBreakRecoveryTimer);
		World->GetTimerManager().ClearTimer(StaggerRecoveryTimer);
	}
	ClearParryTimers();
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
	if (!OwningCharacter.IsValid() || CurrentCombatState == ECombatState::Dead ||
		CurrentCombatState == ECombatState::Finishing || CurrentCombatState == ECombatState::Staggered) return;

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
	ClearParryTimers();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaggerRecoveryTimer);
	}
	if (AnimationState.DefenseState == ECombatDefenseState::GuardBroken)
	{
		EndGuardBreak();
	}
	else if (AnimationState.DefenseState == ECombatDefenseState::Guarding ||
		AnimationState.DefenseState == ECombatDefenseState::Parrying)
	{
		SetGuardMode(false);
	}
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
	if (!OwningCharacter.IsValid() || !IsValid(CurrentWeapon) || bWeaponSwapActive
		|| CurrentCombatState == ECombatState::Dead
		|| AnimationState.DefenseState != ECombatDefenseState::None) return;

	// A successful perfect parry grants one short melee riposte opportunity.
	bool bUseRiposte = false;
	if (bRiposteWindowActive)
	{
		if (CurrentWeapon->GetWeaponType() == EWeaponType::Bow) return;
		bRiposteWindowActive = false;
		bUseRiposte = true;
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RiposteWindowTimer);
	}

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
		const bool bAttackStarted = StartNewCombo();
		bRiposteAttackActive = bUseRiposte && bAttackStarted;
		return;
	}

	if (CurrentCombatState != ECombatState::Attacking && CurrentCombatState != ECombatState::ComboWindow) return;
	if (bTransitionCommittedForWindow) return;

	LastAttackInputTimestamp = World->GetTimeSeconds();
	bHasBufferedAttackInput = true;
	if (bComboWindowOpen) TryQueueComboTransition();
}


void UCombatComponent::HandleAICombatAttack(bool bHeavyAttack)
{
	if (!IsValid(CurrentWeapon)) return;

	const FWeaponConfig& Config = CurrentWeapon->GetConfig();
	int32 AttackCount = 0;
	if (Config.AttackMontageMode == EAttackMontageMode::IndividualAttackMontages)
	{
		AttackCount = Config.IndividualAttackMontages.Num();
	}
	else if (IsValid(Config.AttackComboMontage))
	{
		AttackCount = Config.AttackComboMontage->CompositeSections.Num();
	}

	if (AttackCount <= 0)
	{
		HandleAttackInput();
		return;
	}

	TArray<int32> MatchingAttacks;
	for (int32 Index = 0; Index < AttackCount; ++Index)
	{
		bool bCandidateHeavy = Config.bIsHeavyAttack || !Config.bIsLightAttack;
		if (Config.AttackDamageConfigs.IsValidIndex(Index))
		{
			const FAttackDamageConfig& AttackConfig = Config.AttackDamageConfigs[Index];
			if (AttackConfig.bOverrideAttackType)
			{
				bCandidateHeavy = AttackConfig.bIsHeavyAttack;
			}
		}
		if (bCandidateHeavy == bHeavyAttack)
		{
			MatchingAttacks.Add(Index);
		}
	}

	if (MatchingAttacks.IsEmpty())
	{
		for (int32 Index = 0; Index < AttackCount; ++Index)
		{
			MatchingAttacks.Add(Index);
		}
	}

	RequestedAIStartingAttackIndex = MatchingAttacks[FMath::RandRange(0, MatchingAttacks.Num() - 1)];
	HandleAttackInput();
	RequestedAIStartingAttackIndex = INDEX_NONE;
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
	bParryWindowOpen = false;
	if (!IsValid(CurrentWeapon) || !OwningCharacter.IsValid() || !OwningCharacter->GetMesh()) return false;
	const FWeaponConfig& Config = CurrentWeapon->GetConfig();
	const int32 RequestedIndex = RequestedAIStartingAttackIndex;
	RequestedAIStartingAttackIndex = INDEX_NONE;
	StopActiveAttack(true, true, 0.1f);
	ActiveAttackWeapon = CurrentWeapon;
	PendingAttackIndex = INDEX_NONE;
	ActiveSectionIndex = INDEX_NONE;
	PendingSectionIndex = INDEX_NONE;
	bComboWindowOpen = false;
	bTransitionCommittedForWindow = false;
	ClearInputBuffer();

	if (Config.AttackMontageMode == EAttackMontageMode::IndividualAttackMontages)
	{
		const int32 StartingAttackIndex = Config.IndividualAttackMontages.IsValidIndex(RequestedIndex)
			? RequestedIndex : 0;
		if (!Config.IndividualAttackMontages.IsValidIndex(StartingAttackIndex)
			|| !IsValid(Config.IndividualAttackMontages[StartingAttackIndex]))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Individual montage mode requires a valid starting attack montage."));
			ActiveAttackWeapon.Reset();
			return false;
		}
		return PlayIndividualAttack(StartingAttackIndex);
	}

	UAnimMontage* Montage = Config.AttackComboMontage;
	if (!IsValid(Montage) || Montage->CompositeSections.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Single-montage mode requires a montage with sections."));
		ActiveAttackWeapon.Reset();
		return false;
	}

	const int32 StartingSectionIndex = Montage->CompositeSections.IsValidIndex(RequestedIndex)
		? RequestedIndex : 0;
	UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance();
	if (!Anim) return false;
	ActiveAttackMontage = Montage;
	ActiveAttackToken = ++CurrentAttackToken;
	TotalSectionsInMontage = Montage->CompositeSections.Num();
	ActiveSectionIndex = StartingSectionIndex;
	ActiveAttackIndex = StartingSectionIndex;
	ActiveAttackWeapon->SetActiveAttackIndex(StartingSectionIndex);
	const FName FirstSection = Montage->GetSectionName(StartingSectionIndex);
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
	if (UCombatTargetingComponent* Targeting = OwningCharacter->FindComponentByClass<UCombatTargetingComponent>())
	{
		Targeting->BeginAttackFocus();
	}
	OnComboSectionChanged.Broadcast(StartingSectionIndex, FirstSection);
	return true;
}

bool UCombatComponent::PlayIndividualAttack(int32 AttackIndex)
{
	bParryWindowOpen = false;
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
	ActiveAttackWeapon->SetActiveAttackIndex(AttackIndex);
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
	if (UCombatTargetingComponent* Targeting = OwningCharacter->FindComponentByClass<UCombatTargetingComponent>())
	{
		Targeting->BeginAttackFocus();
	}
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
	ActiveAttackIndex = NewSectionIndex;
	if (ActiveAttackWeapon.IsValid()) ActiveAttackWeapon->SetActiveAttackIndex(NewSectionIndex);
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

	// AI listens for this event and requests the next step only when the authored
	// combo window is open. This keeps progression synchronized with animation timing.
	OnComboWindowOpened.Broadcast();
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
	if (Animation == ActiveAttackMontage && ActiveAttackToken != 0 && ActiveAttackWeapon.IsValid())
	{
		ActiveAttackWeapon->EndTrace();
	}
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

	if (ActiveAttackWeapon.IsValid())
	{
		ActiveAttackWeapon->CancelTrace();
		ActiveAttackWeapon->SetSwingEffectsActive(false);
		ActiveAttackWeapon->SetActiveAttackIndex(INDEX_NONE);
	}
	if (OwningCharacter.IsValid())
	{
		if (UCombatTargetingComponent* Targeting = OwningCharacter->FindComponentByClass<UCombatTargetingComponent>())
		{
			Targeting->EndAttackFocus();
		}
	}
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
	bParryWindowOpen = false;
	bRiposteAttackActive = false;
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

void UCombatComponent::EnterDeadState()
{
	if (CurrentCombatState == ECombatState::Dead)
	{
		return;
	}

	QueuedWeaponSwitch.Reset();
	if (AWeapon_Bow* Bow = Cast<AWeapon_Bow>(CurrentWeapon))
	{
		Bow->CancelDraw();
	}

	// Invalidate any in-flight weapon-swap montage callbacks so they cannot restore Idle
	// after the death transition has completed.
	if (bWeaponSwapActive)
	{
		++WeaponSwapToken;
		bWeaponSwapActive = false;
		WeaponBeingUnequipped.Reset();
		WeaponBeingEquipped.Reset();
	}

	StopActiveAttack(true, true, 0.12f);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardBreakRecoveryTimer);
		World->GetTimerManager().ClearTimer(StaggerRecoveryTimer);
	}
	ClearParryTimers();
	AnimationState.DefenseState = ECombatDefenseState::None;
	SetCombatState(ECombatState::Dead);
}


void UCombatComponent::RegisterEncounterEnemy(AActor* Enemy)
{
	if (!IsValid(Enemy) || Enemy == GetOwner()) return;

	UDamageHandlerComponent* Handler = Enemy->FindComponentByClass<UDamageHandlerComponent>();
	if (!Handler || Handler->bIsDead) return;

	EncounterEnemies.RemoveAll([](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid(); });
	EncounterEnemies.AddUnique(TWeakObjectPtr<AActor>(Enemy));
}

void UCombatComponent::UnregisterEncounterEnemy(AActor* Enemy)
{
	EncounterEnemies.RemoveAll([Enemy](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Enemy;
	});
}

int32 UCombatComponent::GetAliveEncounterEnemyCount() const
{
	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AActor>& Entry : EncounterEnemies)
	{
		AActor* Enemy = Entry.Get();
		if (!IsValid(Enemy)) continue;

		const UDamageHandlerComponent* Handler = Enemy->FindComponentByClass<UDamageHandlerComponent>();
		if (Handler && !Handler->bIsDead) ++AliveCount;
	}
	return AliveCount;
}

bool UCombatComponent::IsEncounterActive() const
{
	return GetAliveEncounterEnemyCount() > 0;
}

bool UCombatComponent::IsLastLivingEncounterEnemy(AActor* Enemy) const
{
	if (!IsValid(Enemy)) return false;

	bool bVictimRegistered = false;
	for (const TWeakObjectPtr<AActor>& Entry : EncounterEnemies)
	{
		AActor* Candidate = Entry.Get();
		if (!IsValid(Candidate)) continue;
		if (Candidate == Enemy)
		{
			bVictimRegistered = true;
			continue;
		}

		const UDamageHandlerComponent* Handler = Candidate->FindComponentByClass<UDamageHandlerComponent>();
		if (Handler && !Handler->bIsDead) return false;
	}
	return bVictimRegistered;
}

void UCombatComponent::EnterFinisherState()
{
	if (CurrentCombatState == ECombatState::Dead || CurrentCombatState == ECombatState::Finishing) return;
	StopActiveAttack(true, true, 0.08f);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardBreakRecoveryTimer);
		World->GetTimerManager().ClearTimer(StaggerRecoveryTimer);
	}
	ClearParryTimers();
	AnimationState.DefenseState = ECombatDefenseState::None;
	SetCombatState(ECombatState::Finishing);
}

void UCombatComponent::ExitFinisherState()
{
	if (CurrentCombatState == ECombatState::Finishing)
	{
		SetCombatState(ECombatState::Idle);
	}
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

void UCombatComponent::SetGuardMode(bool bEnableGuard)
{
	if (bEnableGuard && (CurrentCombatState == ECombatState::Dead ||
		CurrentCombatState == ECombatState::Finishing || CurrentCombatState == ECombatState::Staggered ||
		bWeaponSwapActive || CurrentCombatState == ECombatState::Attacking ||
		CurrentCombatState == ECombatState::ComboWindow || CurrentCombatState == ECombatState::SwappingWeapon ||
		!IsValid(CurrentWeapon)))
	{
		return;
	}

	if (bEnableGuard)
	{
		if (AnimationState.DefenseState != ECombatDefenseState::None) return;
		if (CurrentCombatState == ECombatState::Recovering)
		{
			StopActiveAttack(true, true, 0.08f);
		}
		AnimationState.DefenseState = ECombatDefenseState::Guarding;
		RefreshAnimationState();
		if (UDamageReactionComponent* Reactions = OwningCharacter.IsValid()
			? OwningCharacter->FindComponentByClass<UDamageReactionComponent>() : nullptr)
		{
			Reactions->PlayHitReactionOverride(CurrentWeapon->GetConfig().BeginGuardMontage);
		}
	}
	else if (AnimationState.DefenseState == ECombatDefenseState::Guarding ||
		AnimationState.DefenseState == ECombatDefenseState::Parrying)
	{
		if (AnimationState.DefenseState == ECombatDefenseState::Parrying)
		{
			EndParryAttempt(false);
		}
		AnimationState.DefenseState = ECombatDefenseState::None;
		bParryWindowOpen = false;
		RefreshAnimationState();
	}
}

bool UCombatComponent::BeginParryAttempt()
{
	if (!IsValid(CurrentWeapon) || !OwningCharacter.IsValid() ||
		AnimationState.DefenseState != ECombatDefenseState::Guarding ||
		CurrentCombatState == ECombatState::Dead || CurrentCombatState == ECombatState::Finishing ||
		CurrentCombatState == ECombatState::Staggered || bWeaponSwapActive || !GetWorld() ||
		GetWorld()->GetTimeSeconds() < NextParryAllowedTime)
	{
		return false;
	}

	UAnimMontage* ParryMontage = CurrentWeapon->GetConfig().DefenderParryMontage.Get();
	UDamageReactionComponent* Reactions =
		OwningCharacter->FindComponentByClass<UDamageReactionComponent>();
	if (!IsValid(ParryMontage) || !Reactions || !Reactions->PlayHitReactionOverride(ParryMontage))
	{
		return false;
	}

	SetParryAnimationState(true);
	ParryAttemptStartTime = GetWorld()->GetTimeSeconds();
	const float AttemptDuration = ParryWindowDuration > 0.0f
		? ParryWindowDuration : FMath::Max(0.05f, ParryMontage->GetPlayLength());
	GetWorld()->GetTimerManager().ClearTimer(ParryAttemptTimer);
	GetWorld()->GetTimerManager().SetTimer(ParryAttemptTimer, this,
		&UCombatComponent::HandleParryAttemptExpired, AttemptDuration, false);
	return true;
}

void UCombatComponent::HandleParryAttemptExpired()
{
	EndParryAttempt(false);
}

void UCombatComponent::ClearParryTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParryAttemptTimer);
		World->GetTimerManager().ClearTimer(RiposteWindowTimer);
	}
	ParryAttemptStartTime = -1.0;
	bRiposteWindowActive = false;
	bRiposteAttackActive = false;
}

void UCombatComponent::EndParryAttempt(bool bSuccessful)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParryAttemptTimer);
	}
	ParryAttemptStartTime = -1.0;
	if (AnimationState.DefenseState == ECombatDefenseState::Parrying)
	{
		AnimationState.DefenseState = bSuccessful ? ECombatDefenseState::None : ECombatDefenseState::Guarding;
		RefreshAnimationState();
	}
	if (!bSuccessful && GetWorld())
	{
		NextParryAllowedTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, ParryCooldown);
	}
}

bool UCombatComponent::NotifyParrySucceeded()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bPerfect = ParryAttemptStartTime >= 0.0
		&& (Now - ParryAttemptStartTime) <= FMath::Max(0.0f, PerfectParryWindowDuration);
	EndParryAttempt(true);
	if (bPerfect && GetWorld())
	{
		bRiposteWindowActive = true;
		GetWorld()->GetTimerManager().ClearTimer(RiposteWindowTimer);
		GetWorld()->GetTimerManager().SetTimer(RiposteWindowTimer, [this]()
		{
			bRiposteWindowActive = false;
		}, FMath::Max(0.0f, RiposteWindowDuration), false);
	}
	return bPerfect;
}

bool UCombatComponent::IsWithinGuardArc(const FVector& DirectionToAttacker) const
{
	if (!OwningCharacter.IsValid()) return false;
	const FVector Forward = OwningCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector Direction = DirectionToAttacker.GetSafeNormal2D();
	if (Forward.IsNearlyZero() || Direction.IsNearlyZero()) return false;
	return FVector::DotProduct(Forward, Direction) >= FMath::Cos(FMath::DegreesToRadians(GuardHalfAngleDegrees));
}

bool UCombatComponent::IsWithinParryArc(const FVector& DirectionToAttacker) const
{
	if (!OwningCharacter.IsValid()) return false;
	const FVector Forward = OwningCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector Direction = DirectionToAttacker.GetSafeNormal2D();
	if (Forward.IsNearlyZero() || Direction.IsNearlyZero()) return false;
	return FVector::DotProduct(Forward, Direction) >= FMath::Cos(FMath::DegreesToRadians(ParryHalfAngleDegrees));
}

void UCombatComponent::NotifyParryWindowBegin(const UAnimSequenceBase* Animation)
{
	if (Animation == ActiveAttackMontage && ActiveAttackToken != 0)
	{
		bParryWindowOpen = true;
	}
}

void UCombatComponent::NotifyParryWindowEnd(const UAnimSequenceBase* Animation)
{
	if (Animation == ActiveAttackMontage)
	{
		bParryWindowOpen = false;
	}
}

bool UCombatComponent::CanBeParried() const
{
	if (!IsValid(ActiveAttackMontage) || ActiveAttackToken == 0 || !ActiveAttackWeapon.IsValid()
		|| !OwningCharacter.IsValid() || !OwningCharacter->GetMesh())
	{
		return false;
	}

	const UAnimInstance* Anim = OwningCharacter->GetMesh()->GetAnimInstance();
	if (!Anim || !Anim->Montage_IsPlaying(ActiveAttackMontage))
	{
		return false;
	}

	// If this montage authors a parry notify state, that state is authoritative.
	// Legacy montages without it fall back to their active melee-trace notify window.
	const bool bHasAuthoredParryWindow = ActiveAttackMontage->Notifies.ContainsByPredicate(
		[](const FAnimNotifyEvent& Notify)
		{
			return IsValid(Notify.NotifyStateClass)
				&& Notify.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass());
		});

	return bHasAuthoredParryWindow ? bParryWindowOpen : ActiveAttackWeapon->IsTracing();
}

void UCombatComponent::TriggerGuardBreak(const FVector& ImpactLocation)
{
	if (AnimationState.DefenseState != ECombatDefenseState::Guarding &&
		AnimationState.DefenseState != ECombatDefenseState::Parrying)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardBreakRecoveryTimer);
	}

	ClearParryTimers();
	AnimationState.DefenseState = ECombatDefenseState::GuardBroken;
	bParryWindowOpen = false;
	RefreshAnimationState();
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (AEnemyAIController* AIController = Cast<AEnemyAIController>(Enemy->GetController()))
		{
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->PauseLogic(TEXT("Enemy guard broken"));
			}
		}
	}

	UAnimMontage* BreakMontage = IsValid(CurrentWeapon)
		? CurrentWeapon->GetConfig().BreakGuardMontage.Get() : nullptr;
	bool bMontageStarted = false;
	if (UDamageReactionComponent* Reactions = OwningCharacter.IsValid()
		? OwningCharacter->FindComponentByClass<UDamageReactionComponent>() : nullptr)
	{
		if (IsValid(BreakMontage))
		{
			bMontageStarted = Reactions->PlayHitReactionOverride(BreakMontage);
		}
		const FVector FeedbackLocation = ImpactLocation.IsNearlyZero() && OwningCharacter.IsValid()
			? OwningCharacter->GetActorLocation() : ImpactLocation;
		Reactions->PlayGuardBreakFeedback(CurrentWeapon, FeedbackLocation);
	}

	const float RecoveryDuration = bMontageStarted
		? FMath::Max(0.05f, BreakMontage->GetPlayLength())
		: FMath::Max(0.05f, GuardBreakFallbackDuration);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(GuardBreakRecoveryTimer, this,
			&UCombatComponent::HandleGuardBreakRecovery, RecoveryDuration, false);
	}
}

void UCombatComponent::HandleGuardBreakRecovery()
{
	EndGuardBreak();
}

void UCombatComponent::EndGuardBreak()
{
	if (AnimationState.DefenseState != ECombatDefenseState::GuardBroken) return;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardBreakRecoveryTimer);
	}
	AnimationState.DefenseState = ECombatDefenseState::None;
	RefreshAnimationState();
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (!Enemy->bIsInDamageReaction)
		{
			if (AEnemyAIController* AIController = Cast<AEnemyAIController>(Enemy->GetController()))
			{
				if (UBrainComponent* Brain = AIController->GetBrainComponent())
				{
					Brain->ResumeLogic(TEXT("Enemy guard-break recovery complete"));
				}
			}
		}
	}
}

void UCombatComponent::EnterStaggered(float RecoveryDuration)
{
	if (CurrentCombatState == ECombatState::Dead || CurrentCombatState == ECombatState::Finishing)
	{
		return;
	}

	StopActiveAttack(true, true, 0.08f);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaggerRecoveryTimer);
		World->GetTimerManager().ClearTimer(GuardBreakRecoveryTimer);
	}
	ClearParryTimers();
	AnimationState.DefenseState = ECombatDefenseState::None;
	SetCombatState(ECombatState::Staggered);
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (AEnemyAIController* AIController = Cast<AEnemyAIController>(Enemy->GetController()))
		{
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->PauseLogic(TEXT("Enemy poise broken"));
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(StaggerRecoveryTimer, this,
			&UCombatComponent::HandleStaggerRecovery,
			FMath::Max(0.05f, RecoveryDuration), false);
	}
}

void UCombatComponent::HandleStaggerRecovery()
{
	if (CurrentCombatState == ECombatState::Staggered)
	{
		SetCombatState(ECombatState::Idle);
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
		{
			if (!Enemy->bIsInDamageReaction)
			{
				if (AEnemyAIController* AIController = Cast<AEnemyAIController>(Enemy->GetController()))
				{
					if (UBrainComponent* Brain = AIController->GetBrainComponent())
					{
						Brain->ResumeLogic(TEXT("Enemy stagger recovery complete"));
					}
				}
			}
		}
	}
}

void UCombatComponent::SetParryAnimationState(bool bIsParrying)
{
	if (bIsParrying)
	{
		if (AnimationState.DefenseState == ECombatDefenseState::Guarding)
		{
			AnimationState.DefenseState = ECombatDefenseState::Parrying;
			RefreshAnimationState();
		}
	}
	else if (AnimationState.DefenseState == ECombatDefenseState::Parrying)
	{
		AnimationState.DefenseState = ECombatDefenseState::Guarding;
		RefreshAnimationState();
	}
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
	case ECombatState::Finishing:
		AnimationState.Action = ECombatAnimationAction::Attacking;
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
