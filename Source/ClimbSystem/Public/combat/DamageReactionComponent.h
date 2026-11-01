// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "DamageReactionComponent.generated.h"

class UAnimMontage;
class UDamageHandlerComponent;

/** A data-driven hit reaction with optional weapon, body-region, and direction filters. */
USTRUCT(BlueprintType)
struct FDamageReactionMontage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	bool bMatchWeaponType = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions", meta = (EditCondition = "bMatchWeaponType"))
	EWeaponType WeaponType = EWeaponType::Sword;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	bool bMatchHitLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions", meta = (EditCondition = "bMatchHitLocation"))
	EDamageHitLocation HitLocation = EDamageHitLocation::Torso;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	bool bMatchHitDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions", meta = (EditCondition = "bMatchHitDirection"))
	EDamageHitDirection HitDirection = EDamageHitDirection::Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	TObjectPtr<UAnimMontage> Montage = nullptr;
};

/**
 * Plays character-specific hit and death montages in response to DamageHandlerComponent.
 * Configure the reaction table on the character Blueprint; exact matches outrank broad fallbacks.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UDamageReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageReactionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	TArray<FDamageReactionMontage> HitReactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	TArray<FDamageReactionMontage> DeathReactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions", meta = (ClampMin = "0.0"))
	float HitReactionBlendOutTime = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions", meta = (ClampMin = "0.0"))
	float DeathBlendOutTime = 0.12f;

	/** Clears the death latch and stops the active reaction, e.g. when resetting a test dummy. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Reactions")
	void ResetReactionState();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDamageHandlerComponent> BoundDamageHandler = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveReactionMontage = nullptr;

	bool bDeathHandled = false;

	UFUNCTION()
	void HandleDamageTaken(float ActualDamage, AActor* InstigatorActor,
		const FDamageInfo& DamageInfo, const FHitResult& HitResult);

	UFUNCTION()
	void HandleDeath(AActor* KillerActor);

	UAnimMontage* FindBestHitReaction(const FDamageInfo& DamageInfo) const;
	UAnimMontage* FindBestReaction(const TArray<FDamageReactionMontage>& ReactionSet,
		const FDamageInfo& DamageInfo) const;
	void PlayReaction(UAnimMontage* Montage, float PreviousMontageBlendOutTime);
};
