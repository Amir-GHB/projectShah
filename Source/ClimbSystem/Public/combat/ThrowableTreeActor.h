// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThrowableTreeActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class ACombatCharacterBase;
class AEnemyCharacter;
class UAnimMontage;

/**
 * Temporary environmental throw test actor.
 * Place one in the level, assign a mesh, and let the player throw it at the
 * nearest visible enemy when standing close enough.
 */
UCLASS()
class CLIMBSYSTEM_API AThrowableTreeActor : public AActor
{
	GENERATED_BODY()

public:
	AThrowableTreeActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UStaticMesh> TreeMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Throw")
	TObjectPtr<UAnimMontage> ThrowMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Throw")
	FName ThrowerHandSocketName = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Throw")
	FName ThrowableGripSocketName = TEXT("GripSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable|Interaction", meta = (ClampMin = "50.0"))
	float InteractionRange = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable|Throw", meta = (ClampMin = "100.0"))
	float ThrowSpeed = 1900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable|Throw", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpwardArc = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable|Damage", meta = (ClampMin = "0.0"))
	float ImpactDamage = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable|Damage", meta = (ClampMin = "0.0"))
	float ImpactPoiseDamage = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable|Throw", meta = (ClampMin = "0.1"))
	float ThrownLifeSeconds = 8.0f;

	UFUNCTION(BlueprintPure, Category = "Throwable")
	bool CanBeThrown() const { return !bHasBeenThrown && !bIsAttachedToHand && IsValid(MeshComponent); }

	/** Starts the configured throw montage. Attachment and release are driven by animation notifies. */
	bool BeginThrowSequence(ACombatCharacterBase* Thrower);
	bool AttachToThrowerHand(ACombatCharacterBase* Thrower);
	bool ReleaseFromThrowerHand(ACombatCharacterBase* Thrower);

	/** Selects the closest enemy in the thrower's forward view and launches this actor. */
	bool ThrowAtClosestVisibleEnemy(ACombatCharacterBase* Thrower);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

private:
	bool bHasBeenThrown = false;
	bool bIsAttachedToHand = false;
	TWeakObjectPtr<ACombatCharacterBase> ThrowingCharacter;
	void StopAfterImpact();
};
