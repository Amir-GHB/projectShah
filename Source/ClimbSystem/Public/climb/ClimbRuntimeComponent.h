// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "climb/ClimbRouteTypes.h"
#include "ClimbRuntimeComponent.generated.h"

class AClimbRouteActor;
class ACharacter;

/** Runtime state exposed for animation and future gameplay-system coordination. */
UENUM(BlueprintType)
enum class EClimbRuntimeState : uint8
{
    Inactive,
    Attached,
    MovingAlongSection,
    ExecutingTraversal
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnClimbRuntimeStateChanged,
    EClimbRuntimeState, PreviousState,
    EClimbRuntimeState, NewState);

/**
 * Runtime traversal for the new AClimbRouteActor graph.
 *
 * This component deliberately does not depend on the legacy AClimbRoute or
 * spline-based navigation. Add it to an ACharacter (or a derived character),
 * call SetClimbInput from the owning input layer, and call StartClimbing /
 * StopClimbing from gameplay input.
 */
UCLASS(ClassGroup=(Traversal), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UClimbRuntimeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UClimbRuntimeComponent();

    /** Attach to the closest usable authored node within MaxAttachDistance. */
    UFUNCTION(BlueprintCallable, Category="Climbing|Runtime")
    bool StartClimbing();

    /** Leave the route and restore the character's previous movement mode. */
    UFUNCTION(BlueprintCallable, Category="Climbing|Runtime")
    void StopClimbing();

    /** Surface-relative input: X = left/right, Y = up/down. Values are clamped. */
    UFUNCTION(BlueprintCallable, Category="Climbing|Runtime")
    void SetClimbInput(FVector2D Input);

    UFUNCTION(BlueprintPure, Category="Climbing|Runtime")
    bool IsClimbing() const { return RuntimeState != EClimbRuntimeState::Inactive; }

    UFUNCTION(BlueprintPure, Category="Climbing|Runtime")
    EClimbRuntimeState GetRuntimeState() const { return RuntimeState; }

    UFUNCTION(BlueprintPure, Category="Climbing|Runtime")
    FGuid GetCurrentNodeId() const { return CurrentNodeId; }

    UFUNCTION(BlueprintPure, Category="Climbing|Runtime")
    AClimbRouteActor* GetCurrentRoute() const { return CurrentRoute.Get(); }

    /** Re-scan route actors, useful after routes are spawned dynamically. */
    UFUNCTION(BlueprintCallable, Category="Climbing|Runtime")
    void RefreshAvailableRoutes();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Runtime",
        meta=(ClampMin="1.0"))
    float MaxAttachDistance = 220.0f;

    /** Additional clearance between the character capsule and authored node frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Runtime",
        meta=(ClampMin="0.0"))
    float AttachmentClearance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Movement",
        meta=(ClampMin="1.0"))
    float ClimbSpeed = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Movement",
        meta=(ClampMin="0.0"))
    float InputDirectionThreshold = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Debug")
    bool bDrawRuntimeDebug = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient,
        Category="Climbing|Runtime")
    EClimbRuntimeState RuntimeState = EClimbRuntimeState::Inactive;

    /** Broadcast when traversal starts, ends, or changes between runtime phases. */
    UPROPERTY(BlueprintAssignable, Category="Climbing|Events")
    FOnClimbRuntimeStateChanged OnRuntimeStateChanged;

private:
    struct FRuntimeConnection
    {
        TWeakObjectPtr<AClimbRouteActor> Route;
        FGuid TargetNodeId;
        EClimbTraversalType Type = EClimbTraversalType::ClimbTransition;
        bool bIsTraversalLink = false;
        float Duration = 0.0f;
        float ArcHeight = 0.0f;
    };

    void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    bool FindClosestAttachNode(AClimbRouteActor*& OutRoute,
        FGuid& OutNodeId, FTransform& OutNodeTransform) const;
    bool GetCharacterLocationAtNode(AClimbRouteActor* Route,
        const FGuid& NodeId, FVector& OutLocation,
        FRotator& OutRotation) const;
    bool FindBestConnection(FRuntimeConnection& OutConnection) const;
    void BeginConnection(const FRuntimeConnection& Connection);
    void FinishConnection();
    void SetRuntimeState(EClimbRuntimeState NewState);
    FVector GetSurfaceUp(const FTransform& NodeTransform) const;
    FRotator GetClimbingRotation(const FTransform& NodeTransform) const;

    TArray<TWeakObjectPtr<AClimbRouteActor>> AvailableRoutes;
    TWeakObjectPtr<AClimbRouteActor> CurrentRoute;
    FGuid CurrentNodeId;

    TWeakObjectPtr<AClimbRouteActor> TargetRoute;
    FGuid TargetNodeId;
    EClimbTraversalType ActiveTraversalType = EClimbTraversalType::ClimbTransition;
    bool bActiveConnectionIsTraversalLink = false;
    float ActiveTraversalDuration = 0.0f;
    float ActiveTraversalArcHeight = 0.0f;
    float ActiveTraversalElapsed = 0.0f;
    FVector TraversalStartLocation = FVector::ZeroVector;
    FVector TraversalEndLocation = FVector::ZeroVector;
    FRotator TraversalStartRotation = FRotator::ZeroRotator;
    FRotator TraversalEndRotation = FRotator::ZeroRotator;

    FVector2D ClimbInput = FVector2D::ZeroVector;
    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
};