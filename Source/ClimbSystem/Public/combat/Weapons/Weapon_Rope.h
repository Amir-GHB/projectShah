// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "combat/WeaponBase.h"
#include "Weapon_Rope.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UAnimMontage;
class UAnimSequenceBase;
class UStaticMesh;

/**
 * Rope weapon. The weapon actor stays attached to the character's hand while
 * its spline-rendered rope extends from that attachment point during the swing.
 *
 * The first implementation intentionally handles launch, extension and
 * collision stopping only. Pulling, grappling, return behavior and damage are
 * separate gameplay decisions.
 */
UCLASS(Blueprintable, ClassGroup=(Combat))
class CLIMBSYSTEM_API AWeapon_Rope : public AWeaponBase
{
	GENERATED_BODY()

public:
	AWeapon_Rope();

	virtual void AttachToHand(ACharacter* TargetCharacter) override;
	virtual void AttachToHolster(ACharacter* TargetCharacter) override;

	/** Called by the authored release notify on this weapon's CharacterSwingMontage. */
	void BeginSwing();
	void ReleaseRope(const UAnimSequenceBase* SourceAnimation);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Animation")
	TObjectPtr<UAnimMontage> CharacterSwingMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual")
	TObjectPtr<UStaticMesh> RopeSegmentMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual", meta = (ClampMin = "1", ClampMax = "24"))
	int32 RopeSegmentCount = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Throw", meta = (ClampMin = "100.0"))
	float MaxRopeLength = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Throw", meta = (ClampMin = "100.0"))
	float ExtensionSpeed = 2600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Collision", meta = (ClampMin = "1.0"))
	float RopeCollisionRadius = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Collision")
	TEnumAsByte<ECollisionChannel> RopeTraceChannel = ECC_Visibility;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|State")
	bool bIsExtending = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	static constexpr int32 MaxRopeSegments = 24;

	UPROPERTY(VisibleAnywhere, Category = "Rope|Components")
	TObjectPtr<USplineComponent> RopeSpline = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> RopeSegments;

	FVector RopeStartLocation = FVector::ZeroVector;
	FVector RopeEndLocation = FVector::ZeroVector;
	FVector RopeDirection = FVector::ForwardVector;
	float CurrentRopeLength = 0.0f;
	bool bSwingActive = false;

	void ResetRopeVisual();
	void UpdateRopeVisual();
	bool ResolveThrowDirection(FVector& OutDirection) const;
};
