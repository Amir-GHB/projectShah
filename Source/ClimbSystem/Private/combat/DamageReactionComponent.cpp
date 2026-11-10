// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/DamageReactionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"
#include "combat/WeaponBase.h"
#include "combat/EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"

UDamageReactionComponent::UDamageReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDamageReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		BoundDamageHandler = Owner->FindComponentByClass<UDamageHandlerComponent>();
	}

	if (BoundDamageHandler)
	{
		BoundDamageHandler->OnDamageTaken.AddDynamic(
			this, &UDamageReactionComponent::HandleDamageTaken);
		BoundDamageHandler->OnDeath.AddDynamic(
			this, &UDamageReactionComponent::HandleDeath);
	}
}

void UDamageReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bFinisherActive && IsValid(FinisherCameraActor) && ActiveFinisherCameraShot.bFollowCharacters)
	{
		UpdateFinisherCamera();
	}
}

void UDamageReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundDamageHandler)
	{
		BoundDamageHandler->OnDamageTaken.RemoveDynamic(
			this, &UDamageReactionComponent::HandleDamageTaken);
		BoundDamageHandler->OnDeath.RemoveDynamic(
			this, &UDamageReactionComponent::HandleDeath);
	}
	EndFinisher(true);
	EndParryWindow();
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FinisherCameraDestroyTimer);
	DestroyFinisherCamera();
	Super::EndPlay(EndPlayReason);
}

UAnimMontage* UDamageReactionComponent::FindBestHitReaction(
	const FDamageInfo& DamageInfo) const
{
	return FindBestReaction(HitReactions, DamageInfo);
}

UAnimMontage* UDamageReactionComponent::FindBestReaction(
	const TArray<FDamageReactionMontage>& ReactionSet,
	const FDamageInfo& DamageInfo) const
{
	const FDamageReactionMontage* BestMatch = nullptr;
	int32 BestSpecificity = INDEX_NONE;

	for (const FDamageReactionMontage& Candidate : ReactionSet)
	{
		if (!IsValid(Candidate.Montage))
		{
			continue;
		}

		if (Candidate.bMatchWeaponType && Candidate.WeaponType != DamageInfo.WeaponType)
		{
			continue;
		}
		if (Candidate.bMatchHitLocation && Candidate.HitLocation != DamageInfo.HitLocation)
		{
			continue;
		}
		if (Candidate.bMatchHitDirection && Candidate.HitDirection != DamageInfo.HitDirection)
		{
			continue;
		}

		const int32 Specificity = static_cast<int32>(Candidate.bMatchWeaponType)
			+ static_cast<int32>(Candidate.bMatchHitLocation)
			+ static_cast<int32>(Candidate.bMatchHitDirection);

		if (Specificity > BestSpecificity)
		{
			BestSpecificity = Specificity;
			BestMatch = &Candidate;
		}
	}

	return BestMatch ? BestMatch->Montage.Get() : nullptr;
}

void UDamageReactionComponent::PlayReaction(UAnimMontage* Montage, float PreviousMontageBlendOutTime)
{
	if (!IsValid(Montage))
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || !Character->GetMesh())
	{
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	if (IsValid(ActiveReactionMontage) && AnimInstance->Montage_IsPlaying(ActiveReactionMontage))
	{
		AnimInstance->Montage_Stop(FMath::Max(0.0f, PreviousMontageBlendOutTime), ActiveReactionMontage);
	}

	const float PlayedDuration = AnimInstance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
	if (PlayedDuration > 0.0f)
	{
		ActiveReactionMontage = Montage;
	}
}

void UDamageReactionComponent::PlayHitReactionOverride(UAnimMontage* Montage)
{
	if (!bDeathHandled && IsValid(Montage))
	{
		PlayReaction(Montage, HitReactionBlendOutTime);
	}
}

void UDamageReactionComponent::HandleDamageTaken(
	float ActualDamage, AActor* InstigatorActor,
	const FDamageInfo& DamageInfo, const FHitResult& HitResult)
{
	if (bDeathHandled || ActualDamage <= 0.0f || DamageInfo.bIsDamageOverTime)
	{
		return;
	}

	if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(GetOwner()))
	{
		Mage->CancelPendingMagicAttack();
	}
	PlayReaction(FindBestHitReaction(DamageInfo), HitReactionBlendOutTime);
}

void UDamageReactionComponent::BeginParryWindow(float Duration)
{
	if (!GetWorld() || bDeathHandled) return;
	UCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!Combat || !Combat->IsGuarding()) return;
	bParryWindowActive = true;
	Combat->SetParryAnimationState(true);
	PlayReaction(DefaultParryAttemptMontage, HitReactionBlendOutTime);
	GetWorld()->GetTimerManager().ClearTimer(ParryWindowTimer);
	GetWorld()->GetTimerManager().SetTimer(ParryWindowTimer, this,
		&UDamageReactionComponent::EndParryWindow, FMath::Max(0.05f, Duration > 0.0f ? Duration : ParryWindowDuration), false);
}

void UDamageReactionComponent::EndParryWindow()
{
	bParryWindowActive = false;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ParryWindowTimer);
	if (UCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		Combat->SetParryAnimationState(false);
	}
}

bool UDamageReactionComponent::TryParry(AActor* Attacker, AWeaponBase* AttackingWeapon, const FHitResult& HitResult)
{
	if (!bParryWindowActive || bDeathHandled || !IsValid(Attacker) || !IsValid(AttackingWeapon) || !GetOwner()) return false;
	UCombatComponent* DefenderCombat = GetOwner()->FindComponentByClass<UCombatComponent>();
	if (!DefenderCombat || !DefenderCombat->IsGuarding()) return false;
	EndParryWindow();
	AWeaponBase* DefenderWeapon = DefenderCombat->CurrentWeapon;
	const FWeaponConfig& Config = IsValid(DefenderWeapon)
		? DefenderWeapon->GetConfig() : AttackingWeapon->GetConfig();
	UAnimMontage* DefenderMontage = IsValid(Config.DefenderParryMontage)
		? Config.DefenderParryMontage.Get() : DefaultDefenderParryMontage.Get();
	UAnimMontage* AttackerMontage = IsValid(Config.AttackerParriedMontage)
		? Config.AttackerParriedMontage.Get() : DefaultAttackerParriedMontage.Get();
	PlayReaction(DefenderMontage, HitReactionBlendOutTime);
	FDamageInfo ParryDamage;
	ParryDamage.BaseDamage = Config.ParryDamage;
	ParryDamage.DamageCategory = EDamageCategory::Physical;
	ParryDamage.DamageCauser = GetOwner();
	ParryDamage.InstigatorWeapon = DefenderWeapon;
	ParryDamage.WeaponType = IsValid(DefenderWeapon) ? DefenderWeapon->GetWeaponType() : EWeaponType::Unarmed;
	ParryDamage.DamageDirection = (Attacker->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
	ParryDamage.Normalize();
	FHitResult ParryHit;
	ParryHit.Location = Attacker->GetActorLocation();
	ParryHit.ImpactPoint = Attacker->GetActorLocation();
	if (UDamageHandlerComponent* AttackerDamage = Attacker->FindComponentByClass<UDamageHandlerComponent>())
	{
		AttackerDamage->ApplyDamage(GetOwner(), Attacker, ParryDamage, ParryHit);
	}
	else
	{
		FPointDamageEvent Event(ParryDamage.GetCalculatedRawDamage(), ParryHit, FVector::UpVector, nullptr);
		Attacker->TakeDamage(ParryDamage.GetCalculatedRawDamage(), Event, GetOwner()->GetInstigatorController(), GetOwner());
	}
	const UDamageHandlerComponent* AttackerDamageState =
		Attacker->FindComponentByClass<UDamageHandlerComponent>();
		if ((!AttackerDamageState || !AttackerDamageState->bIsDead))
	{
		// Cancel the active attack transaction as well as replacing its pose.
		// This closes weapon traces and invalidates pending combo/montage callbacks.
		if (UCombatComponent* AttackerCombat = Attacker->FindComponentByClass<UCombatComponent>())
		{
			AttackerCombat->StopActiveAttack(true, true, 0.04f);
		}
		if (AMagicEnemyCharacter* Mage = Cast<AMagicEnemyCharacter>(Attacker))
		{
			Mage->CancelPendingMagicAttack();
		}
		if (UDamageReactionComponent* AttackerReaction = Attacker->FindComponentByClass<UDamageReactionComponent>())
		{
			AttackerReaction->PlayReaction(AttackerMontage, 0.04f);
		}
	}
	return true;
}

bool UDamageReactionComponent::ResolveIncomingMeleeHit(AActor* Attacker, AWeaponBase* AttackingWeapon, const FHitResult& HitResult)
{
	if (!IsValid(Attacker) || !IsValid(AttackingWeapon)) return false;

	// A successful timed parry consumes the incoming hit before VFX or damage are dispatched.
	if (TryParry(Attacker, AttackingWeapon, HitResult)) return true;

	UCombatComponent* DefenderCombat = GetOwner()
		? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr;
	if (DefenderCombat && DefenderCombat->IsGuarding() && AttackingWeapon->GetConfig().bIsLightAttack)
	{
		// Current standard melee attacks are light attacks: they break guard but deal no health damage.
		DefenderCombat->TriggerGuardBreak();
		return true;
	}

	return false;
}

void UDamageReactionComponent::HandleDeath(AActor* KillerActor)
{
	if (bDeathHandled)
	{
		return;
	}

	FDamageInfo DeathDamageInfo;
	if (BoundDamageHandler)
	{
		DeathDamageInfo = BoundDamageHandler->LastDamageInfo;
	}

	// The attacker owns the simple encounter list and coordinates the paired animation.
	if (IsValid(KillerActor))
	{
		UCombatComponent* KillerCombat = KillerActor->FindComponentByClass<UCombatComponent>();
		if (KillerCombat && KillerCombat->IsLastLivingEncounterEnemy(GetOwner()))
		{
			UDamageReactionComponent* KillerReactions =
				KillerActor->FindComponentByClass<UDamageReactionComponent>();
			if (KillerReactions && KillerReactions->TryPlayFinisher(GetOwner(), DeathDamageInfo))
			{
				bDeathHandled = true;
				KillerCombat->UnregisterEncounterEnemy(GetOwner());
				return;
			}
		}

		if (KillerCombat)
		{
			KillerCombat->UnregisterEncounterEnemy(GetOwner());
		}
	}

	if (bFinisherActive)
	{
		EndFinisher(true);
	}
	bDeathHandled = true;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCombatComponent* Combat = Character->FindComponentByClass<UCombatComponent>())
		{
			Combat->EnterDeadState();
		}

		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->DisableMovement();
		}
	}

	UAnimMontage* SelectedDeathMontage = FindBestReaction(DeathReactions, DeathDamageInfo);
	UAnimMontage* FinalDeathMontage = IsValid(SelectedDeathMontage) ? SelectedDeathMontage.Get() : DeathMontage.Get();
	PlayReaction(FinalDeathMontage, DeathBlendOutTime);
	if (bRagdollOnDeath)
	{
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
				if (IsValid(FinalDeathMontage) && Anim && Anim->Montage_IsPlaying(FinalDeathMontage))
		{
			// Hand off at the start of the final blend-out, before the AnimBP restores locomotion.
			FOnMontageBlendingOutStarted BlendOutDelegate;
			BlendOutDelegate.BindLambda([this](UAnimMontage*, bool) { ActivateRagdoll(); });
			Anim->Montage_SetBlendingOutDelegate(BlendOutDelegate, FinalDeathMontage);
		}
		else
		{
			ActivateRagdoll();
		}
	}
}

bool UDamageReactionComponent::TryPlayFinisher(AActor* Victim, const FDamageInfo& KillingBlow)
{
	ACharacter* AttackerCharacter = Cast<ACharacter>(GetOwner());
	ACharacter* VictimCharacter = Cast<ACharacter>(Victim);
	if (bFinisherActive || !IsValid(AttackerCharacter) || !IsValid(VictimCharacter)
		|| KillingBlow.bIsDamageOverTime || KillingBlow.WeaponType == EWeaponType::Bow
		|| !KillingBlow.InstigatorWeapon.IsValid())
	{
		return false;
	}

	// Finisher eligibility is based on combat state and encounter registration, not controller type.
	APlayerController* PlayerController = Cast<APlayerController>(AttackerCharacter->GetController());
	UCombatComponent* AttackerCombat = AttackerCharacter->FindComponentByClass<UCombatComponent>();
	UDamageReactionComponent* VictimReactions = VictimCharacter->FindComponentByClass<UDamageReactionComponent>();
	UDamageHandlerComponent* VictimDamage = VictimCharacter->FindComponentByClass<UDamageHandlerComponent>();
	if (!AttackerCombat || !VictimReactions || !VictimDamage
		|| !VictimDamage->bIsDead || !AttackerCombat->IsLastLivingEncounterEnemy(Victim))
	{
		return false;
	}

	EFinisherDirection IncomingDirection = EFinisherDirection::All;
	switch (KillingBlow.HitDirection)
	{
	case EDamageHitDirection::Front: IncomingDirection = EFinisherDirection::Front; break;
	case EDamageHitDirection::Back: IncomingDirection = EFinisherDirection::Back; break;
	case EDamageHitDirection::Left: IncomingDirection = EFinisherDirection::Left; break;
	case EDamageHitDirection::Right: IncomingDirection = EFinisherDirection::Right; break;
	default: break;
	}

	TArray<const FFinisherMontagePair*> ExactDirectionPairs;
	TArray<const FFinisherMontagePair*> UniversalPairs;
	for (const FFinisherMontagePair& Pair : FinisherMontages)
	{
		if (Pair.WeaponType != KillingBlow.WeaponType
			|| !IsValid(Pair.AttackerMontage) || !IsValid(Pair.VictimMontage))
		{
			continue;
		}
		if (Pair.Direction == IncomingDirection && IncomingDirection != EFinisherDirection::All)
		{
			ExactDirectionPairs.Add(&Pair);
		}
		else if (Pair.Direction == EFinisherDirection::All)
		{
			UniversalPairs.Add(&Pair);
		}
	}

	const TArray<const FFinisherMontagePair*>& MatchingPairs =
		ExactDirectionPairs.IsEmpty() ? UniversalPairs : ExactDirectionPairs;
	if (MatchingPairs.IsEmpty())
	{
		return false;
	}

	const FFinisherMontagePair* SelectedPair = MatchingPairs[FMath::RandRange(0, MatchingPairs.Num() - 1)];
	UAnimInstance* AttackerAnim = AttackerCharacter->GetMesh()
		? AttackerCharacter->GetMesh()->GetAnimInstance() : nullptr;
	UAnimInstance* VictimAnim = VictimCharacter->GetMesh()
		? VictimCharacter->GetMesh()->GetAnimInstance() : nullptr;
	if (!AttackerAnim || !VictimAnim)
	{
		return false;
	}

	AttackerCombat->EnterFinisherState();
	bFinisherActive = true;
	FinisherVictim = VictimCharacter;
	FinisherController = PlayerController;
	if (PlayerController)
	{
		PreviousViewTarget = PlayerController->GetViewTarget();
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
	}
	ActiveFinisherAttackerMontage = SelectedPair->AttackerMontage;
	ActiveFinisherVictimMontage = SelectedPair->VictimMontage;
	bAttackerFinisherEnded = false;
	bVictimFinisherEnded = false;

	// Set both participant targets before either montage starts. The anchored participant's
	// target is its current transform, allowing its own notify window to cancel root-motion drift.
	if (SelectedPair->bUseMotionWarping)
	{
		UMotionWarpingComponent* AttackerWarping = AttackerCharacter->FindComponentByClass<UMotionWarpingComponent>();
		UMotionWarpingComponent* VictimWarping = VictimCharacter->FindComponentByClass<UMotionWarpingComponent>();
		if (!IsValid(AttackerWarping) || !IsValid(VictimWarping)
			|| SelectedPair->AttackerWarpTargetName.IsNone() || SelectedPair->VictimWarpTargetName.IsNone())
		{
			EndFinisher(true);
			return false;
		}

		const FTransform AttackerWorld = AttackerCharacter->GetActorTransform();
		const FTransform VictimWorld = VictimCharacter->GetActorTransform();
		FTransform AttackerTarget = AttackerWorld;
		FTransform VictimTarget = VictimWorld;

		if (SelectedPair->WarpSubject == EFinisherWarpSubject::Attacker)
		{
			// Authored transform is VictimRelativeToAttacker. Invert it to position the attacker
			// relative to the anchored victim.
			const FTransform AttackerRelativeToVictim = SelectedPair->VictimRelativeToAttacker.Inverse();
			AttackerTarget = AttackerRelativeToVictim * VictimWorld;
		}
		else
		{
			// Keep the attacker anchored and move the victim to the authored relative transform.
			VictimTarget = SelectedPair->VictimRelativeToAttacker * AttackerWorld;
		}

		AttackerWarping->AddOrUpdateWarpTargetFromTransform(SelectedPair->AttackerWarpTargetName, AttackerTarget);
		VictimWarping->AddOrUpdateWarpTargetFromTransform(SelectedPair->VictimWarpTargetName, VictimTarget);
		ActiveFinisherAttackerWarpingComponent = AttackerWarping;
		ActiveFinisherVictimWarpingComponent = VictimWarping;
		ActiveFinisherAttackerWarpTargetName = SelectedPair->AttackerWarpTargetName;
		ActiveFinisherVictimWarpTargetName = SelectedPair->VictimWarpTargetName;
	}
	const float AttackerDuration = AttackerAnim->Montage_Play(ActiveFinisherAttackerMontage);
	if (AttackerDuration <= 0.0f)
	{
		EndFinisher(true);
		return false;
	}

	const float VictimDuration = VictimAnim->Montage_Play(ActiveFinisherVictimMontage);
	if (VictimDuration <= 0.0f)
	{
		EndFinisher(true);
		return false;
	}

	FOnMontageEnded AttackerEndDelegate;
	AttackerEndDelegate.BindUObject(this, &UDamageReactionComponent::HandleFinisherMontageEnded);
	AttackerAnim->Montage_SetEndDelegate(AttackerEndDelegate, ActiveFinisherAttackerMontage);

	FOnMontageEnded VictimEndDelegate;
	VictimEndDelegate.BindUObject(this, &UDamageReactionComponent::HandleFinisherMontageEnded);
	VictimAnim->Montage_SetEndDelegate(VictimEndDelegate, ActiveFinisherVictimMontage);
	// A paired finisher victim needs the same pose-preserving physics handoff as a normal death.
	TWeakObjectPtr<UDamageReactionComponent> WeakVictimReactions(VictimReactions);
	FOnMontageBlendingOutStarted VictimBlendOutDelegate;
	VictimBlendOutDelegate.BindLambda([WeakVictimReactions](UAnimMontage*, bool)
	{
		if (WeakVictimReactions.IsValid()) WeakVictimReactions->ActivateRagdoll();
	});
	VictimAnim->Montage_SetBlendingOutDelegate(VictimBlendOutDelegate, ActiveFinisherVictimMontage);

	// The victim's normal death montage is suppressed; the paired finisher montage owns the death pose.
	VictimReactions->bDeathHandled = true;
	VictimReactions->ActiveReactionMontage = ActiveFinisherVictimMontage;
	if (UCombatComponent* VictimCombat = VictimCharacter->FindComponentByClass<UCombatComponent>())
	{
		VictimCombat->EnterDeadState();
	}
	if (UCharacterMovementComponent* VictimMovement = VictimCharacter->GetCharacterMovement())
	{
		VictimMovement->StopMovementImmediately();
		// Flying mode keeps CharacterMovement active for root-motion montages without gravity.
		VictimMovement->SetMovementMode(MOVE_Flying);
	}
	if (AController* VictimController = VictimCharacter->GetController())
	{
		VictimController->StopMovement();
		VictimController->SetIgnoreMoveInput(true);
	}

	// Cinematic presentation is opt-in per montage pair. Otherwise the gameplay camera is untouched.
	if (SelectedPair->bUseCinematicCamera)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FinisherCameraDestroyTimer);
			DestroyFinisherCamera();
			ActiveFinisherCameraShot = SelectedPair->CameraShot;
			ActiveFinisherCameraBlendOutTime = ActiveFinisherCameraShot.BlendOutTime;
			FinisherCameraActor = World->SpawnActor<ACameraActor>();
			if (FinisherCameraActor)
			{
				if (UCameraComponent* Camera = FinisherCameraActor->GetCameraComponent())
				{
					Camera->SetFieldOfView(FMath::Clamp(ActiveFinisherCameraShot.FieldOfView, 5.0f, 150.0f));
				}
				UpdateFinisherCamera();
				SetComponentTickEnabled(ActiveFinisherCameraShot.bFollowCharacters);
				if (PlayerController)
				{
					PlayerController->SetViewTargetWithBlend(FinisherCameraActor,
						FMath::Max(0.0f, ActiveFinisherCameraShot.BlendInTime));
				}
			}
		}
	}

	return true;
}

void UDamageReactionComponent::HandleFinisherMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bFinisherActive) return;

	if (bInterrupted)
	{
		EndFinisher(true);
		return;
	}

	if (Montage == ActiveFinisherAttackerMontage)
	{
		bAttackerFinisherEnded = true;
	}
	else if (Montage == ActiveFinisherVictimMontage)
	{
		bVictimFinisherEnded = true;
	}

	if (bAttackerFinisherEnded && bVictimFinisherEnded)
	{
		EndFinisher(false);
	}
}

void UDamageReactionComponent::EndFinisher(bool bStopMontages)
{
	if (!bFinisherActive) return;
	bFinisherActive = false;

	ACharacter* AttackerCharacter = Cast<ACharacter>(GetOwner());
	ACharacter* VictimCharacter = FinisherVictim.Get();

	if (!bStopMontages && IsValid(VictimCharacter))
	{
		if (UDamageReactionComponent* VictimReactions = VictimCharacter->FindComponentByClass<UDamageReactionComponent>())
		{
			if (VictimReactions->bRagdollOnDeath) VictimReactions->ActivateRagdoll();
		}
	}

	if (bStopMontages)
	{
		if (AttackerCharacter && AttackerCharacter->GetMesh())
		{
			if (UAnimInstance* Anim = AttackerCharacter->GetMesh()->GetAnimInstance())
			{
				if (IsValid(ActiveFinisherAttackerMontage))
				{
					Anim->Montage_Stop(0.1f, ActiveFinisherAttackerMontage);
				}
			}
		}
		if (VictimCharacter && VictimCharacter->GetMesh())
		{
			if (UAnimInstance* Anim = VictimCharacter->GetMesh()->GetAnimInstance())
			{
				if (IsValid(ActiveFinisherVictimMontage))
				{
					Anim->Montage_Stop(0.1f, ActiveFinisherVictimMontage);
				}
			}
		}
	}

	SetComponentTickEnabled(false);
	if (APlayerController* Controller = FinisherController.Get())
	{
		// Only request a camera blend when this finisher actually switched to a cinematic camera.
		if (IsValid(FinisherCameraActor))
		{
			if (AActor* RestoreTarget = PreviousViewTarget.Get())
			{
				Controller->SetViewTargetWithBlend(RestoreTarget, ActiveFinisherCameraBlendOutTime);
			}
		}
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
	}

	// Remove only the two targets owned by this finisher. Other combat warp targets remain untouched.
	if (ActiveFinisherAttackerWarpingComponent.IsValid() && !ActiveFinisherAttackerWarpTargetName.IsNone())
	{
		ActiveFinisherAttackerWarpingComponent->RemoveWarpTarget(ActiveFinisherAttackerWarpTargetName);
	}
	if (ActiveFinisherVictimWarpingComponent.IsValid() && !ActiveFinisherVictimWarpTargetName.IsNone())
	{
		ActiveFinisherVictimWarpingComponent->RemoveWarpTarget(ActiveFinisherVictimWarpTargetName);
	}
	ActiveFinisherAttackerWarpingComponent.Reset();
	ActiveFinisherVictimWarpingComponent.Reset();
	ActiveFinisherAttackerWarpTargetName = NAME_None;
	ActiveFinisherVictimWarpTargetName = NAME_None;

	if (IsValid(FinisherCameraActor))
	{
		if (GetWorld() && ActiveFinisherCameraBlendOutTime > 0.0f && FinisherController.IsValid())
		{
			GetWorld()->GetTimerManager().SetTimer(
				FinisherCameraDestroyTimer, this, &UDamageReactionComponent::DestroyFinisherCamera,
				ActiveFinisherCameraBlendOutTime, false);
		}
		else
		{
			DestroyFinisherCamera();
		}
	}

	if (AttackerCharacter)
	{
		if (UCombatComponent* Combat = AttackerCharacter->FindComponentByClass<UCombatComponent>())
		{
			Combat->ExitFinisherState();
		}
	}

	FinisherVictim.Reset();
	FinisherController.Reset();
	PreviousViewTarget.Reset();
	ActiveFinisherAttackerMontage = nullptr;
	ActiveFinisherVictimMontage = nullptr;
	bAttackerFinisherEnded = false;
	bVictimFinisherEnded = false;
}

void UDamageReactionComponent::UpdateFinisherCamera()
{
	ACharacter* AttackerCharacter = Cast<ACharacter>(GetOwner());
	ACharacter* VictimCharacter = FinisherVictim.Get();
	if (!IsValid(FinisherCameraActor) || !IsValid(AttackerCharacter) || !IsValid(VictimCharacter))
	{
		return;
	}

	const FVector Midpoint = (AttackerCharacter->GetActorLocation()
		+ VictimCharacter->GetActorLocation()) * 0.5f;
	const FVector Offset = VictimCharacter->GetActorTransform().TransformVectorNoScale(
		ActiveFinisherCameraShot.RelativeOffset);
	const FVector CameraLocation = Midpoint + Offset;

	FVector LookAtLocation = Midpoint;
	switch (ActiveFinisherCameraShot.LookAt)
	{
	case EFinisherCameraLookAt::Attacker:
		LookAtLocation = AttackerCharacter->GetActorLocation();
		break;
	case EFinisherCameraLookAt::Victim:
		LookAtLocation = VictimCharacter->GetActorLocation();
		break;
	default:
		break;
	}
	LookAtLocation += ActiveFinisherCameraShot.LookAtOffset;
	FinisherCameraActor->SetActorLocationAndRotation(
		CameraLocation, (LookAtLocation - CameraLocation).Rotation());
}

void UDamageReactionComponent::DestroyFinisherCamera()
{
	if (IsValid(FinisherCameraActor))
	{
		FinisherCameraActor->Destroy();
	}
	FinisherCameraActor = nullptr;
}

void UDamageReactionComponent::ActivateRagdoll()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || !Character->GetMesh() || Character->GetMesh()->IsSimulatingPhysics()) return;

	// The blending-out delegate fires while the death montage still owns the pose.
	// Enable full-body simulation at that handoff; the mesh then remains in physics control.
	Character->GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	Character->GetMesh()->SetAllBodiesSimulatePhysics(true);
	Character->GetMesh()->SetSimulatePhysics(true);
	Character->GetMesh()->WakeAllRigidBodies();
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
}

void UDamageReactionComponent::ResetReactionState()
{
	EndFinisher(true);
	EndParryWindow();
	bDeathHandled = false;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (Character->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				if (IsValid(ActiveReactionMontage))
				{
					AnimInstance->Montage_Stop(0.0f, ActiveReactionMontage);
				}
			}
		}
	}

	ActiveReactionMontage = nullptr;
}
