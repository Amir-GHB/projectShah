// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "CombatTypes.h"
#include "DamageReactionComponent.generated.h"

class UAnimMontage;
class UDamageHandlerComponent;
class ACharacter;
class ACameraActor;
class APlayerController;

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

/** Camera aim target for an optional authored finisher shot. */
UENUM(BlueprintType)
enum class EFinisherCameraLookAt : uint8
{
    Midpoint UMETA(DisplayName = "Between Characters"),
    Attacker UMETA(DisplayName = "Attacker"),
    Victim UMETA(DisplayName = "Victim")
};

/** Per-finisher camera shot. Offset is relative to the victim: X forward, Y right, Z up. */
USTRUCT(BlueprintType)
struct FFinisherCameraShot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera")
    FVector RelativeOffset = FVector(-320.0f, 0.0f, 145.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera")
    EFinisherCameraLookAt LookAt = EFinisherCameraLookAt::Midpoint;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera")
    FVector LookAtOffset = FVector(0.0f, 0.0f, 85.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera", meta = (ClampMin = "5.0", ClampMax = "150.0"))
    float FieldOfView = 55.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera", meta = (ClampMin = "0.0"))
    float BlendInTime = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera", meta = (ClampMin = "0.0"))
    float BlendOutTime = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera")
    bool bFollowCharacters = true;
};

/** A paired execution animation with optional direction filtering and an optional custom camera shot. */
USTRUCT(BlueprintType)
struct FFinisherMontagePair
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers")
    EWeaponType WeaponType = EWeaponType::Sword;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers")
    EFinisherDirection Direction = EFinisherDirection::All;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers")
    TObjectPtr<UAnimMontage> AttackerMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers")
    TObjectPtr<UAnimMontage> VictimMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera")
    bool bUseCinematicCamera = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers|Camera", meta = (EditCondition = "bUseCinematicCamera"))
    FFinisherCameraShot CameraShot;
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

	/** Tries to execute a paired finisher against Victim. Called by the victim's death reaction. */
	bool TryPlayFinisher(AActor* Victim, const FDamageInfo& KillingBlow);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	TArray<FDamageReactionMontage> HitReactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	TArray<FDamageReactionMontage> DeathReactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Reactions")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Finishers")
	TArray<FFinisherMontagePair> FinisherMontages;

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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDamageHandlerComponent> BoundDamageHandler = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveReactionMontage = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> FinisherVictim;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> FinisherController;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PreviousViewTarget;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> FinisherCameraActor = nullptr;

	FFinisherCameraShot ActiveFinisherCameraShot;
	float ActiveFinisherCameraBlendOutTime = 0.45f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveFinisherAttackerMontage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveFinisherVictimMontage = nullptr;

	bool bFinisherActive = false;
	bool bAttackerFinisherEnded = false;
	bool bVictimFinisherEnded = false;
	FTimerHandle FinisherCameraDestroyTimer;

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

	void HandleFinisherMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void EndFinisher(bool bStopMontages);
	void DestroyFinisherCamera();
	void UpdateFinisherCamera();
};
