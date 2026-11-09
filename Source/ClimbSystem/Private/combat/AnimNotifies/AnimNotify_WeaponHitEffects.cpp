// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/AnimNotify_WeaponHitEffects.h"
#include "combat/CombatComponent.h"
#include "combat/WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"

void UAnimNotify_WeaponHitEffects::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	UCombatComponent* Combat = Character ? Character->FindComponentByClass<UCombatComponent>() : nullptr;
	AWeaponBase* Weapon = Combat ? Combat->CurrentWeapon : nullptr;
	if (!IsValid(Weapon) || !IsValid(Weapon->WeaponMesh)) return;

	FVector Location = Weapon->WeaponMesh->GetComponentLocation();
	if (!EffectSocketName.IsNone())
	{
		if (Weapon->WeaponMesh->DoesSocketExist(EffectSocketName))
		{
			Location = Weapon->WeaponMesh->GetSocketTransform(EffectSocketName, RTS_World).GetLocation();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponHitEffects] Socket '%s' does not exist on weapon mesh '%s'. Using weapon mesh origin."),
				*EffectSocketName.ToString(), *GetNameSafe(Weapon->WeaponMesh->GetStaticMesh()));
		}
	}
	FHitResult SyntheticHit;
	SyntheticHit.Location = Location;
	SyntheticHit.ImpactPoint = Location;
	SyntheticHit.ImpactNormal = -Character->GetActorForwardVector();
	Weapon->PlayHitEffects(SyntheticHit);
}
