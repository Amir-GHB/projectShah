// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponHitEffects.generated.h"

/** Plays the currently equipped weapon's configured impact Niagara/sound at an authored weapon socket. */
UCLASS(meta=(DisplayName="Weapon Hit Effects"))
class CLIMBSYSTEM_API UAnimNotify_WeaponHitEffects : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon VFX")
	FName EffectSocketName = NAME_None;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
