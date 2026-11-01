// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/DamageReactionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

void UDamageReactionComponent::PlayReaction(UAnimMontage* Montage, float BlendInTime)
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
		AnimInstance->Montage_Stop(FMath::Max(0.0f, BlendInTime), ActiveReactionMontage);
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

	PlayReaction(FindBestHitReaction(DamageInfo), HitReactionBlendIn);
}

void UDamageReactionComponent::HandleDeath(AActor* KillerActor)
{
	if (bDeathHandled)
	{
		return;
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

	FDamageInfo DeathDamageInfo;
	if (BoundDamageHandler)
	{
		DeathDamageInfo = BoundDamageHandler->LastDamageInfo;
	}

	UAnimMontage* SelectedDeathMontage = FindBestReaction(DeathReactions, DeathDamageInfo);
	PlayReaction(IsValid(SelectedDeathMontage) ? SelectedDeathMontage : DeathMontage, DeathBlendIn);
}
