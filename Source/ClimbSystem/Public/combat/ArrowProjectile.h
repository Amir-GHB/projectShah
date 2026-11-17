// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "combat/CombatProjectile.h"
#include "ArrowProjectile.generated.h"

/**
 * Dedicated physical arrow projectile.
 * Unlike magic projectiles, an arrow survives its first impact and embeds in the
 * exact component it struck (including skeletal mesh components).
 */
UCLASS(Blueprintable)
class CLIMBSYSTEM_API AArrowProjectile : public ACombatProjectile
{
	GENERATED_BODY()

public:
	AArrowProjectile();

	/** How long an embedded arrow remains before it is cleaned up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Impact", meta = (ClampMin = "0.0"))
	float EmbeddedLifeSeconds = 25.0f;

	/**
	 * Distance from the projectile actor origin to the arrow tip, measured along local +X.
	 * Set this to match the arrow mesh pivot so the tip reaches the impact point.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Impact", meta = (ClampMin = "0.0"))
	float ArrowTipOffset = 35.0f;

	/** Pushes the arrow tip slightly into the struck surface for a convincing embed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Impact", meta = (ClampMin = "0.0"))
	float EmbedDepth = 6.0f;

	/** Attach to the component hit. Disable to leave the arrow at the impact location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Impact")
	bool bAttachToHitComponent = true;

protected:
	virtual void BeginPlay() override;


	virtual void HandleProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit) override;

private:
	void EmbedAtImpact(UPrimitiveComponent* OtherComponent, const FHitResult& Hit);
	bool IsFriendlyEnemyHit(AActor* OtherActor) const;
	bool bIsEmbedded = false;
};
