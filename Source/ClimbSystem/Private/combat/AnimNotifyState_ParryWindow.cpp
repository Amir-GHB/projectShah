#include "combat/AnimNotifyState_ParryWindow.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "combat/DamageReactionComponent.h"

void UAnimNotifyState_ParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UDamageReactionComponent* Reactions = Owner ? Owner->FindComponentByClass<UDamageReactionComponent>() : nullptr)
	{
		Reactions->BeginParryWindow(TotalDuration);
	}
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UDamageReactionComponent* Reactions = Owner ? Owner->FindComponentByClass<UDamageReactionComponent>() : nullptr)
	{
		Reactions->EndParryWindow();
	}
}
