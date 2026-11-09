// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotifyState_WeaponSwingEffects.h"

#include "combat/CombatComponent.h"
#include "combat/WeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_WeaponSwingEffects::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	UCombatComponent* Combat = Character ? Character->FindComponentByClass<UCombatComponent>() : nullptr;
	AWeaponBase* Weapon = Combat ? Combat->CurrentWeapon : nullptr;
	if (!IsValid(Weapon)) return;

	// The sound is a one-shot at the start; Niagara remains attached and active for the notify window.
	Weapon->PlaySwingEffects(bPlaySound, false);
	Weapon->SetSwingEffectsActive(bPlayNiagara);
}

void UAnimNotifyState_WeaponSwingEffects::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	UCombatComponent* Combat = Character ? Character->FindComponentByClass<UCombatComponent>() : nullptr;
	AWeaponBase* Weapon = Combat ? Combat->CurrentWeapon : nullptr;
	if (IsValid(Weapon))
	{
		Weapon->SetSwingEffectsActive(false);
	}
}
