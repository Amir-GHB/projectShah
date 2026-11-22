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
class AEnemyCharacter;
class UCharacterMovementComponent;

/**
 * Code-driven thrown rope weapon. The hand is a fixed simulation anchor while
 * a Verlet particle chain is paid out, thrown and simulated under gravity.
 */
UCLASS(Blueprintable, ClassGroup=(Combat))
class CLIMBSYSTEM_API AWeapon_Rope : public AWeaponBase
{
	GENERATED_BODY()

public:
	AWeapon_Rope();

	virtual void AttachToHand(ACharacter* TargetCharacter) override;
	virtual void AttachToHolster(ACharacter* TargetCharacter) override;

	void BeginSwing();
	void ReleaseRope(const UAnimSequenceBase* SourceAnimation);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Animation")
	TObjectPtr<UAnimMontage> CharacterSwingMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual")
	TObjectPtr<UStaticMesh> RopeSegmentMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual", meta = (ClampMin = "1", ClampMax = "24"))
	int32 RopeSegmentCount = 16;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Throw", meta = (ClampMin = "100.0"))
	float MaxRopeLength = 1400.0f;

	/** Rate at which rope is fed out after release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Throw", meta = (ClampMin = "100.0"))
	float ExtensionSpeed = 1800.0f;

	/** Initial velocity imparted to newly deployed rope particles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Throw", meta = (ClampMin = "100.0"))
	float ThrowSpeed = 2200.0f;

	/** Length deliberately left as loose rope at the end of the throw. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Throw", meta = (ClampMin = "0.0"))
	float EndSlackLength = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Physics")
	float GravityScale = 1.0f;

	/** Damps particle velocity slightly each simulation step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityDamping = 0.015f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Physics", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ConstraintIterations = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Collision", meta = (ClampMin = "1.0"))
	float RopeCollisionRadius = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Collision")
	TEnumAsByte<ECollisionChannel> RopeTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Pull", meta = (ClampMin = "100.0"))
	float EnemyPullSpeed = 1250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Pull", meta = (ClampMin = "0.0"))
	float PullStopPadding = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Pull", meta = (ClampMin = "1.0"))
	float RopeContractionSpeed = 8.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|State")
	bool bIsExtending = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	static constexpr int32 MaxRopeSegments = 24;
	static constexpr float SimulationStep = 1.0f / 60.0f;

	UPROPERTY(VisibleAnywhere, Category = "Rope|Components")
	TObjectPtr<USplineComponent> RopeSpline = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> RopeSegments;

	TArray<FVector> ParticlePositions;
	TArray<FVector> PreviousParticlePositions;
	TArray<bool> bParticleDeployed;
	TArray<FVector> ParticleDeploymentPositions;

	FVector RopeStartLocation = FVector::ZeroVector;
	FVector LastAnchorLocation = FVector::ZeroVector;
	FVector AnchorVelocity = FVector::ZeroVector;
	FVector RopeDirection = FVector::ForwardVector;
	FVector RopeEndLocation = FVector::ZeroVector;

	float CurrentRopeLength = 0.0f;
	float SimulationAccumulator = 0.0f;
	bool bSwingActive = false;
	bool bSimulatingRope = false;
	float TargetThrowDistance = 0.0f;

	TWeakObjectPtr<AEnemyCharacter> PullTarget;
	TEnumAsByte<EMovementMode> SavedTargetMovementMode = MOVE_Walking;
	uint8 SavedTargetCustomMovementMode = 0;
	float SavedTargetMaxWalkSpeed = 0.0f;
	bool bSavedTargetPauseAnims = false;
	bool bIsPullingTarget = false;
	bool bTargetAIWasPaused = false;

	void ResetRopeVisual();
	bool TryBeginTargetPull();
	void BeginTargetPull(AEnemyCharacter* Target);
	void UpdateTargetPull(float DeltaSeconds);
	void RestorePulledTarget();
	AActor* GetTargetingActor() const;
	void InitializeRopeSimulation(const FVector& Direction);
	void SimulateRopeStep(float StepSeconds);
	void SolveRopeConstraints();
	void ResolveParticleCollisions();
	void UpdateRopeVisual();
	bool ResolveThrowDirection(FVector& OutDirection) const;
};
