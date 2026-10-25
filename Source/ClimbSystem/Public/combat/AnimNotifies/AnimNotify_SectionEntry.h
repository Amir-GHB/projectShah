// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SectionEntry.generated.h"

/**
 * AnimNotify placed at the start frame of a montage section to confirm section progression.
 */
UCLASS(meta = (DisplayName = "Combo Section Entry"))
class CLIMBSYSTEM_API UAnimNotify_SectionEntry : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Name of the section this notify belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FName SectionName;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
