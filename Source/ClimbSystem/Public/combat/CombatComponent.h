// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "CombatTypes.h"
#include "CombatComponent.generated.h"

class AWeaponBase;
class UDamageHandlerComponent;
class UAnimMontage;
class ACharacter;
class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponSwitchedSignature, EWeaponType, NewWeaponType, AWeaponBase*, NewWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboSectionChangedSignature, int32, SectionIndex, FName, SectionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChangedSignature, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttackEndedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnComboWindowOpenedSignature);

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

	/** Enters or leaves the sustained guard stance. Call this from the owning character Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
	void SetGuardMode(bool bEnableGuard);

	/** Starts a weapon-specific parry attempt while guard is held. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
	bool BeginParryAttempt();

	/** Notify-state callbacks used by authored attack-montage parry windows. */
	void NotifyParryWindowBegin(const UAnimSequenceBase* Animation);
	void NotifyParryWindowEnd(const UAnimSequenceBase* Animation);

	/** True only while this character's active attack is inside a valid parry window. */
	bool CanBeParried() const;
	bool IsParryAttemptActive() const { return AnimationState.DefenseState == ECombatDefenseState::Parrying; }
	void EndParryAttempt(bool bSuccessful);
	void NotifyParrySucceeded();
	bool IsWithinGuardArc(const FVector& DirectionToAttacker) const;
	bool IsWithinParryArc(const FVector& DirectionToAttacker) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
	float ParryWindowDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
	float ParryCooldown = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
	float PerfectParryWindowDuration = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float GuardHalfAngleDegrees = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ParryHalfAngleDegrees = 70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense")
	bool bHeavyHitsBypassGuard = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense")
	bool bCriticalHitsBypassGuard = false;

	/** Called when an incoming hit breaks an active guard. Plays the equipped weapon's break montage. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
	void TriggerGuardBreak();
	void EnterStaggered(float RecoveryDuration);

	/** Optional early recovery hook; C++ also ends guard break from its montage/fallback timer. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
	void EndGuardBreak();

	UFUNCTION(BlueprintPure, Category = "Combat|Defense")
	bool IsGuarding() const { return AnimationState.DefenseState == ECombatDefenseState::Guarding || AnimationState.DefenseState == ECombatDefenseState::Parrying; }

	/** Used by DamageReactionComponent to expose the parry attempt to the AnimBP. */
	void SetParryAnimationState(bool bIsParrying);

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

	/** Interrupt combat and enter the terminal Dead state. */
	UFUNCTION(BlueprintCallable, Category = "Combat|State")
	void EnterDeadState();

	/** Registers an enemy for the lightweight player encounter/last-enemy check. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Encounter")
	void RegisterEncounterEnemy(AActor* Enemy);

	/** Removes an enemy from the lightweight encounter list. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Encounter")
	void UnregisterEncounterEnemy(AActor* Enemy);

	UFUNCTION(BlueprintPure, Category = "Combat|Encounter")
	int32 GetAliveEncounterEnemyCount() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Encounter")
	bool IsLastLivingEncounterEnemy(AActor* Enemy) const;

	UFUNCTION(BlueprintPure, Category = "Combat|Encounter")
	bool IsEncounterActive() const;

	/** Temporarily blocks normal combat input while this character performs a finisher. */
	void EnterFinisherState();
	void ExitFinisherState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	ECombatState CurrentCombatState = ECombatState::Idle;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> EncounterEnemies;

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

	/** Broadcast after the combo window opens, allowing AI to request the next combo step. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnComboWindowOpenedSignature OnComboWindowOpened;

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
	bool bParryWindowOpen = false;
	FTimerHandle GuardBreakRecoveryTimer;
	FTimerHandle StaggerRecoveryTimer;
	FTimerHandle ParryAttemptTimer;
	FTimerHandle RiposteWindowTimer;
	double ParryAttemptStartTime = -1.0;
	double NextParryAllowedTime = 0.0;
	bool bRiposteWindowActive = false;
	bool bRiposteAttackActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "0.0"))
	float RiposteWindowDuration = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "1.0"))
	float RiposteDamageMultiplier = 1.5f;

	void HandleParryAttemptExpired();
	void ClearParryTimers();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Defense", meta = (ClampMin = "0.05"))
	float GuardBreakFallbackDuration = 1.0f;

	void HandleGuardBreakRecovery();
	void HandleStaggerRecovery();
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
