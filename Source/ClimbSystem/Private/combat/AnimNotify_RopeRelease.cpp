// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotify_RopeRelease.h"

#include "Components/SkeletalMeshComponent.h"
#include "combat/CombatComponent.h"
#include "combat/Weapons/Weapon_Rope.h"

void UAnimNotify_RopeRelease::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Character = MeshComp ? MeshComp->GetOwner() : nullptr;
	UCombatComponent* Combat = Character
		? Character->FindComponentByClass<UCombatComponent>()
		: nullptr;
	AWeapon_Rope* Rope = Combat ? Cast<AWeapon_Rope>(Combat->CurrentWeapon) : nullptr;

	if (IsValid(Rope))
	{
		Rope->ReleaseRope(Animation);
	}
}
