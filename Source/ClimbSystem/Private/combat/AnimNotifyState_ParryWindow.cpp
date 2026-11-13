#include "combat/AnimNotifyState_ParryWindow.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "combat/CombatComponent.h"

void UAnimNotifyState_ParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		Combat->NotifyParryWindowBegin(Animation);
	}
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		Combat->NotifyParryWindowEnd(Animation);
	}
}
