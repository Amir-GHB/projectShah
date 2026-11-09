// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotify_MagicProjectileRelease.h"
#include "combat/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_MagicProjectileRelease::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AMagicEnemyCharacter* Mage = MeshComp ? Cast<AMagicEnemyCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		Mage->ReleaseMagicProjectile();
	}
}
