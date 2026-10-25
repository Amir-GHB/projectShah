// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "combat/CombatTypes.h"
#include "AnimNotify_WeaponSwapPoint.generated.h"

/** Place at the exact frame the old weapon is holstered or the new weapon is drawn. */
UCLASS(meta = (DisplayName = "Weapon Swap Point"))
class CLIMBSYSTEM_API UAnimNotify_WeaponSwapPoint : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapon Swap")
	EWeaponSwapNotifyAction Action = EWeaponSwapNotifyAction::HolsterCurrent;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
