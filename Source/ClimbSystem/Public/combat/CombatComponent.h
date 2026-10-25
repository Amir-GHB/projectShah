// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "CombatComponent.generated.h"

class AWeaponBase;
class UAnimMontage;
class ACharacter;
class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponSwitchedSignature, EWeaponType, NewWeaponType, AWeaponBase*, NewWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboSectionChangedSignature, int32, SectionIndex, FName, SectionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChangedSignature, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttackEndedSignature);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapons")
	TMap<EWeaponType, TSubclassOf<AWeaponBase>> WeaponClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapons")
	TArray<EWeaponType> WeaponCycleOrder = { EWeaponType::Sword, EWeaponType::BullHeadHammer, EWeaponType::Spear, EWeaponType::Bow, EWeaponType::Unarmed };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapons")
	EWeaponType DefaultWeaponType = EWeaponType::Sword;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Weapons")
	TMap<EWeaponType, TObjectPtr<AWeaponBase>> SpawnedWeapons;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Weapons")
	TObjectPtr<AWeaponBase> CurrentWeapon = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	EWeaponType CurrentWeaponType = EWeaponType::Unarmed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapons")
	bool bQueueWeaponSwitchDuringAttack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Buffer", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float InputBufferDuration = 0.35f;

	UFUNCTION(BlueprintCallable, Category = "Combat|Weapons")
	void EquipWeapon(EWeaponType NewWeaponType);

	UFUNCTION(BlueprintCallable, Category = "Combat|Weapons")
	void SwitchToNextWeapon();

	UFUNCTION(BlueprintPure, Category = "Combat|Weapons")
	AWeaponBase* GetWeapon(EWeaponType Type) const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void HandleAttackInput();

	/** Completes a held ranged attack and releases its projectile. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ranged")
	void HandleAttackReleased();

	/** Cancels a held ranged attack without firing, for canceled input or interruption. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ranged")
	void CancelAttackInput();

	/** Returns the current animation snapshot, including live bow draw strength. */
	UFUNCTION(BlueprintPure, Category = "Combat|Animation")
	FCombatAnimationState GetAnimationState() const;

	/** Animation-only override for entering/leaving a noncombat stance. Does not change weapon inventory. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	void SetNonCombatAnimation(bool bEnableNonCombatAnimation);

	void NotifyComboWindowBegin(const UAnimSequenceBase* Animation);
	void NotifyComboWindowEnd(const UAnimSequenceBase* Animation);
	/** Starts a queued individual-montage combo step at an authored transition frame. */
	void NotifyComboTransitionPoint(const UAnimSequenceBase* Animation);
	void NotifyMeleeTraceBegin(const UAnimSequenceBase* Animation);
	void NotifyMeleeTraceTick(const UAnimSequenceBase* Animation, float FrameDeltaTime);
	void NotifyMeleeTraceEnd(const UAnimSequenceBase* Animation);

	UFUNCTION(BlueprintCallable, Category = "Combat|Combo")
	void NotifySectionEntered(FName SectionName, const UAnimSequenceBase* Animation);

	/** Called by UAnimNotify_WeaponSwapPoint. */
	void NotifyWeaponSwapPoint(EWeaponSwapNotifyAction Action, const UAnimSequenceBase* Animation);

	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void ResetCombo();

	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void StopActiveAttack(bool bInterrupted = true, bool bStopMontage = true, float BlendOutTime = 0.2f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	ECombatState CurrentCombatState = ECombatState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	int32 ActiveSectionIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	int32 PendingSectionIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	int32 TotalSectionsInMontage = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	int32 ActiveAttackIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	int32 PendingAttackIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bComboWindowOpen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bTransitionCommittedForWindow = false;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnWeaponSwitchedSignature OnWeaponSwitched;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnComboSectionChangedSignature OnComboSectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnCombatStateChangedSignature OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnAttackEndedSignature OnAttackEnded;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<AWeaponBase> ActiveAttackWeapon = nullptr;

	uint64 CurrentAttackToken = 0;
	uint64 ActiveAttackToken = 0;
	bool bIsStoppingAttack = false;
	double LastAttackInputTimestamp = -1.0;
	bool bHasBufferedAttackInput = false;
	TOptional<EWeaponType> QueuedWeaponSwitch;

	// Weapon-switch transaction state. Generation tokens reject stale montage callbacks.
	uint64 WeaponSwapToken = 0;
	bool bWeaponSwapActive = false;
	bool bHolsterPointReached = false;
	bool bDrawPointReached = false;
	bool bPlayingUnequipMontage = false;
	EWeaponType PendingWeaponType = EWeaponType::Unarmed;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Animation")
	FCombatAnimationState AnimationState;

	bool bForceNonCombatAnimation = false;
	TWeakObjectPtr<AWeaponBase> WeaponBeingUnequipped;
	TWeakObjectPtr<AWeaponBase> WeaponBeingEquipped;

	void OnMontageInstanceEnded(UAnimMontage* Montage, bool bInterrupted, uint64 AttackToken);
	void OnMontageInstanceBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint64 AttackToken);
	void OnWeaponSwapMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 SwapToken, bool bWasUnequip);
	
	bool StartNewCombo();
	bool PlayIndividualAttack(int32 AttackIndex);
	bool TryQueueComboTransition();
	bool TryQueueSectionTransition();
	void CommitSectionTransition(int32 NewSectionIndex, FName NewSectionName);
	void FinishAttack(bool bInterrupted, bool bStopMontage, float BlendOutTime);
	void BeginWeaponSwap(EWeaponType NewWeaponType);
	void PlayUnequipMontageOrContinue();
	void PlayEquipMontageOrFinish();
	void FinishWeaponSwap();
	void SetCombatState(ECombatState NewState);
	void RefreshAnimationState();
	void ClearInputBuffer();
	bool IsInputBufferValid() const;
};
