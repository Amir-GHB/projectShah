// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotifies/AnimNotify_SectionEntry.h"
#include "Components/SkeletalMeshComponent.h"
#include "combat/CombatComponent.h"

void UAnimNotify_SectionEntry::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->NotifySectionEntered(SectionName, Animation);
	}
}

FString UAnimNotify_SectionEntry::GetNotifyName_Implementation() const
{
	if (!SectionName.IsNone())
	{
		return FString::Printf(TEXT("Section Entry: %s"), *SectionName.ToString());
	}
	return TEXT("Section Entry");
}
