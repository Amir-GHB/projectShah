// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_RopeRelease.generated.h"

/** Releases the equipped rope weapon at the authored frame of its swing montage. */
UCLASS(meta=(DisplayName="Release Rope"))
class CLIMBSYSTEM_API UAnimNotify_RopeRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
