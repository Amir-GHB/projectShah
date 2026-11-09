// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotify_WeaponHitEffects.h"
#include "combat/CombatComponent.h"
#include "combat/WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

void UAnimNotify_WeaponHitEffects::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	UCombatComponent* Combat = Character ? Character->FindComponentByClass<UCombatComponent>() : nullptr;
	AWeaponBase* Weapon = Combat ? Combat->CurrentWeapon : nullptr;
	if (!IsValid(Weapon) || !IsValid(Weapon->WeaponMesh)) return;

	const FVector Location = !EffectSocketName.IsNone() && Weapon->WeaponMesh->DoesSocketExist(EffectSocketName)
		? Weapon->WeaponMesh->GetSocketLocation(EffectSocketName)
		: Weapon->WeaponMesh->GetComponentLocation();
	FHitResult SyntheticHit;
	SyntheticHit.Location = Location;
	SyntheticHit.ImpactPoint = Location;
	SyntheticHit.ImpactNormal = -Character->GetActorForwardVector();
	Weapon->PlayHitEffects(SyntheticHit);
}
