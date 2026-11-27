// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotify_ThrowableRelease.h"
#include "combat/CombatCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_ThrowableRelease::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (ACombatCharacterBase* Character = MeshComp
		? Cast<ACombatCharacterBase>(MeshComp->GetOwner()) : nullptr)
	{
		Character->ReleasePendingThrowable();
	}
}
