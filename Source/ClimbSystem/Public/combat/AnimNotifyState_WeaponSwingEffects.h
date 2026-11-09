// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_WeaponSwingEffects.generated.h"

/** Enables the equipped weapon's attached swing trail only for this animation window. */
UCLASS(meta=(DisplayName="Weapon Swing Effects"))
class CLIMBSYSTEM_API UAnimNotifyState_WeaponSwingEffects : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon VFX")
	bool bPlayNiagara = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon VFX")
	bool bPlaySound = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
