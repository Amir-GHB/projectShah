// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotifies/AnimNotifyState_ComboWindow.h"
#include "Components/SkeletalMeshComponent.h"
#include "combat/CombatComponent.h"

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->NotifyComboWindowBegin(Animation);
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->NotifyComboWindowEnd(Animation);
	}
}
