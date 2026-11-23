// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotifyState_RopeSwing.h"

#include "Components/SkeletalMeshComponent.h"
#include "combat/CombatComponent.h"
#include "combat/Weapons/Weapon_Rope.h"

void UAnimNotifyState_RopeSwing::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UCombatComponent* Combat = Owner
		? Owner->FindComponentByClass<UCombatComponent>()
		: nullptr;
	AWeapon_Rope* Rope = Combat ? Cast<AWeapon_Rope>(Combat->CurrentWeapon) : nullptr;

	if (IsValid(Rope))
	{
		Rope->BeginSwingVisual(TotalDuration);
	}
}

void UAnimNotifyState_RopeSwing::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UCombatComponent* Combat = Owner
		? Owner->FindComponentByClass<UCombatComponent>()
		: nullptr;
	AWeapon_Rope* Rope = Combat ? Cast<AWeapon_Rope>(Combat->CurrentWeapon) : nullptr;

	if (IsValid(Rope))
	{
		Rope->EndSwingVisual();
	}
}
