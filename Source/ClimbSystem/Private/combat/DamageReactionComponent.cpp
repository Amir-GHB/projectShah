// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/DamageReactionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "combat/CombatComponent.h"
#include "combat/DamageHandlerComponent.h"

UDamageReactionComponent::UDamageReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

void UDamageReactionComponent::HandleDamageTaken(
	float ActualDamage, AActor* InstigatorActor,
	const FDamageInfo& DamageInfo, const FHitResult& HitResult)
{
	if (bDeathHandled || ActualDamage <= 0.0f || DamageInfo.bIsDamageOverTime)
	{
		return;
	}

	PlayReaction(FindBestHitReaction(DamageInfo), HitReactionBlendOutTime);
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
	PlayReaction(IsValid(SelectedDeathMontage) ? SelectedDeathMontage : DeathMontage, DeathBlendOutTime);
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

	// The first implementation is player-only and requires a registered, final encounter enemy.
	APlayerController* PlayerController = Cast<APlayerController>(AttackerCharacter->GetController());
	UCombatComponent* AttackerCombat = AttackerCharacter->FindComponentByClass<UCombatComponent>();
	UDamageReactionComponent* VictimReactions = VictimCharacter->FindComponentByClass<UDamageReactionComponent>();
	UDamageHandlerComponent* VictimDamage = VictimCharacter->FindComponentByClass<UDamageHandlerComponent>();
	if (!PlayerController || !AttackerCombat || !VictimReactions || !VictimDamage
		|| !VictimDamage->bIsDead || !AttackerCombat->IsLastLivingEncounterEnemy(Victim))
	{
		return false;
	}

	TArray<const FFinisherMontagePair*> MatchingPairs;
	for (const FFinisherMontagePair& Pair : FinisherMontages)
	{
		if (Pair.WeaponType == KillingBlow.WeaponType
			&& IsValid(Pair.AttackerMontage) && IsValid(Pair.VictimMontage))
		{
			MatchingPairs.Add(&Pair);
		}
	}
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
	PreviousViewTarget = PlayerController->GetViewTarget();
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	ActiveFinisherAttackerMontage = SelectedPair->AttackerMontage;
	ActiveFinisherVictimMontage = SelectedPair->VictimMontage;
	bAttackerFinisherEnded = false;
	bVictimFinisherEnded = false;

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

	// Temporary camera actor for a simple two-character execution shot. Motion Warping is intentionally separate.
	if (UWorld* World = GetWorld())
	{
		FinisherCameraActor = World->SpawnActor<ACameraActor>();
		if (FinisherCameraActor)
		{
			const FVector Midpoint = (AttackerCharacter->GetActorLocation()
				+ VictimCharacter->GetActorLocation()) * 0.5f;
			const FVector AwayFromPair = (AttackerCharacter->GetActorLocation()
				- VictimCharacter->GetActorLocation()).GetSafeNormal();
			const FVector CameraLocation = Midpoint + AwayFromPair * FinisherCameraDistance
				+ FVector::UpVector * FinisherCameraHeight;
			FinisherCameraActor->SetActorLocationAndRotation(
				CameraLocation, (Midpoint - CameraLocation).Rotation());
			PlayerController->SetViewTargetWithBlend(FinisherCameraActor, FinisherCameraBlendTime);
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

	if (APlayerController* Controller = FinisherController.Get())
	{
		if (AActor* RestoreTarget = PreviousViewTarget.Get())
		{
			Controller->SetViewTargetWithBlend(RestoreTarget, FinisherCameraBlendTime);
		}
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
	}

	if (IsValid(FinisherCameraActor))
	{
		FinisherCameraActor->Destroy();
	}
	FinisherCameraActor = nullptr;

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

void UDamageReactionComponent::ResetReactionState()
{
	EndFinisher(true);
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
