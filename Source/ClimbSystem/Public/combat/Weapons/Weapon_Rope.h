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
class UStaticMeshComponent;
class AEnemyCharacter;

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

	/** Static mesh displayed at the currently simulated free end of the rope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual")
	TObjectPtr<UStaticMesh> HookMesh = nullptr;

	/** World-independent scale applied to the hook mesh component. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual")
	FVector HookScale = FVector::OneVector;

	/** Rotation adjustment applied after aligning the hook's forward axis to the rope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual")
	FRotator HookRotationOffset = FRotator::ZeroRotator;

	/** Position adjustment in the hook's rotated local space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Visual")
	FVector HookPositionOffset = FVector::ZeroVector;

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

	/** Test mode: automatically selects the nearest enemy inside the view-direction cone instead of requiring precise aim/lock-on. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Targeting")
	bool bUseNonAimTargeting = false;

	/** Half-angle of the automatic target selection cone, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Targeting", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float NonAimTargetingHalfAngle = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Pull", meta = (ClampMin = "100.0"))
	float EnemyPullSpeed = 1250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Pull", meta = (ClampMin = "0.0"))
	float PullStopPadding = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Pull", meta = (ClampMin = "1.0"))
	float RopeContractionSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rope|Pull", meta = (ClampMin = "100.0"))
	float RopeRetractSpeed = 1800.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|State")
	bool bIsExtending = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	static constexpr int32 MaxRopeSegments = 24;
	static constexpr float SimulationStep = 1.0f / 60.0f;

	UPROPERTY(VisibleAnywhere, Category = "Rope|Components")
	TObjectPtr<USplineComponent> RopeSpline = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Rope|Components")
	TObjectPtr<UStaticMeshComponent> HookMeshComponent = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> RopeSegments;

	TArray<FVector> ParticlePositions;
	TArray<FVector> PreviousParticlePositions;
	TArray<bool> bParticleDeployed;
	TArray<FVector> ParticleDeploymentPositions;
	TArray<FVector> ParticleLaunchVelocities;

	FVector RopeStartLocation = FVector::ZeroVector;
	FVector LastAnchorLocation = FVector::ZeroVector;
	FVector AnchorVelocity = FVector::ZeroVector;
	FVector RopeDirection = FVector::ForwardVector;
	FVector RopeEndLocation = FVector::ZeroVector;
	FVector ThrowAimLocation = FVector::ZeroVector;

	float CurrentRopeLength = 0.0f;
	float SimulationAccumulator = 0.0f;
	bool bSwingActive = false;
	bool bSimulatingRope = false;
	bool bHasThrowAimLocation = false;
	float TargetThrowDistance = 0.0f;

	TWeakObjectPtr<AEnemyCharacter> PullTarget;
	bool bIsPullingTarget = false;
	bool bIsRetracting = false;
	float RetractionLength = 0.0f;
	FVector RetractionDirection = FVector::ForwardVector;

	void ResetRopeVisual();
	bool TryBeginTargetPull();
	void BeginTargetPull(AEnemyCharacter* Target);
	void UpdateTargetPull(float DeltaSeconds);
	void BeginRopeRetraction();
	void UpdateRopeRetraction(float DeltaSeconds);
	AActor* GetTargetingActor() const;
	void InitializeRopeSimulation(const FVector& Direction, const FVector& AimLocation, bool bHasAimLocation);
	void SimulateRopeStep(float StepSeconds);
	void SolveRopeConstraints();
	void ResolveParticleCollisions();
	void UpdateRopeVisual();
	bool ResolveThrowDirection(FVector& OutDirection, FVector& OutAimLocation, AEnemyCharacter*& OutTarget) const;
};
