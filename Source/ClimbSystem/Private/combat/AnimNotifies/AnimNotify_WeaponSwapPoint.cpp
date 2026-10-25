// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotifies/AnimNotify_WeaponSwapPoint.h"
#include "Components/SkeletalMeshComponent.h"
#include "combat/CombatComponent.h"

void UAnimNotify_WeaponSwapPoint::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->NotifyWeaponSwapPoint(Action, Animation);
	}
}

FString UAnimNotify_WeaponSwapPoint::GetNotifyName_Implementation() const
{
	return Action == EWeaponSwapNotifyAction::HolsterCurrent
		? TEXT("Weapon Swap: Holster Current")
		: TEXT("Weapon Swap: Draw Target");
}
