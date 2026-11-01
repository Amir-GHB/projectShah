// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ClimbAnimationMovementNotifyState.generated.h"

/**
 * Opens a movement window for the owning character's ClimbRuntimeComponent.
 * Place this notify state over the part of an animation during which the
 * character should travel toward the pending climbing destination.
 */
UCLASS(meta=(DisplayName="Climb Animation Movement Window"))
class CLIMBSYSTEM_API UClimbAnimationMovementNotifyState : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
