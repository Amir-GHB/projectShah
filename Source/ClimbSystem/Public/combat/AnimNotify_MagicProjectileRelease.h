// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_MagicProjectileRelease.generated.h"

/** Releases the pending mage projectile at the authored frame of the attack montage. */
UCLASS(meta=(DisplayName="Release Magic Projectile"))
class CLIMBSYSTEM_API UAnimNotify_MagicProjectileRelease : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
