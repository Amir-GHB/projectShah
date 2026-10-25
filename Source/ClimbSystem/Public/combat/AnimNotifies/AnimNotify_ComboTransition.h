// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ComboTransition.generated.h"

/**
 * Starts a queued next attack montage at this authored frame.
 * Intended for IndividualAttackMontages mode so combo chains can skip idle tails.
 */
UCLASS(meta = (DisplayName = "Combo Transition Point"))
class CLIMBSYSTEM_API UAnimNotify_ComboTransition : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Combo Transition Point"); }
};
