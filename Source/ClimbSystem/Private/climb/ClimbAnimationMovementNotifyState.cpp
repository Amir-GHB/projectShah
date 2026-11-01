// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbAnimationMovementNotifyState.h"

#include "climb/ClimbRuntimeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UClimbAnimationMovementNotifyState::NotifyBegin(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const float TotalDuration,
     const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    UClimbRuntimeComponent* ClimbComponent = IsValid(Owner)
        ? Owner->FindComponentByClass<UClimbRuntimeComponent>()
        : nullptr;

    if (IsValid(ClimbComponent))
    {
        ClimbComponent->BeginAnimationMovementWindow(TotalDuration);
    }
}

void UClimbAnimationMovementNotifyState::NotifyEnd(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);

    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    UClimbRuntimeComponent* ClimbComponent = IsValid(Owner)
        ? Owner->FindComponentByClass<UClimbRuntimeComponent>()
        : nullptr;

    if (IsValid(ClimbComponent))
    {
        ClimbComponent->EndAnimationMovementWindow();
    }
}
