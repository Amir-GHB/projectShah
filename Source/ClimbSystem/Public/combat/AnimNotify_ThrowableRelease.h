// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ThrowableRelease.generated.h"

/** Releases and launches the pending throwable at the authored montage frame. */
UCLASS(meta=(DisplayName="Release Throwable"))
class CLIMBSYSTEM_API UAnimNotify_ThrowableRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
