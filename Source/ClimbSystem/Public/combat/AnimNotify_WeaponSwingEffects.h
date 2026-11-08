#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponSwingEffects.generated.h"

/** Plays the currently equipped weapon's configured swing effects, including on a missed attack. */
UCLASS(meta=(DisplayName="Weapon Swing Effects"))
class CLIMBSYSTEM_API UAnimNotify_WeaponSwingEffects : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|VFX")
	bool bPlaySound = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|VFX")
	bool bPlayNiagara = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
