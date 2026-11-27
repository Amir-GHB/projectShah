// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ThrowableAttach.generated.h"

/** Attaches the pending environmental throwable to the configured hand/grip sockets. */
UCLASS(meta=(DisplayName="Attach Throwable To Hand"))
class CLIMBSYSTEM_API UAnimNotify_ThrowableAttach : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
