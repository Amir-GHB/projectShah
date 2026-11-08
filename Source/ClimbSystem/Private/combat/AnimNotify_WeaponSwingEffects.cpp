#include "combat/AnimNotify_WeaponSwingEffects.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "combat/CombatComponent.h"
#include "combat/WeaponBase.h"

void UAnimNotify_WeaponSwingEffects::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	UCombatComponent* Combat = Character ? Character->FindComponentByClass<UCombatComponent>() : nullptr;
	AWeaponBase* Weapon = Combat ? Combat->CurrentWeapon : nullptr;
	if (IsValid(Weapon))
	{
		Weapon->PlaySwingEffects(bPlaySound, bPlayNiagara);
	}
}
