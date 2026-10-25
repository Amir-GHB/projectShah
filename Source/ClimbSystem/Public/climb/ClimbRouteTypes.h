// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ClimbRouteTypes.generated.h"

class UAnimSequence;
class UPrimitiveComponent;

/** Authoring categories for meaningful locations in a climbing network. */
UENUM(BlueprintType)
enum class EClimbRouteNodeType : uint8
{
    Standard,
    Junction,
    Entry,
    Exit,
    TraversalAnchor
};

/** The first implementation uses straight polyline segments.
 * CurvedSegments is reserved for a future path representation; it is not implemented yet.
 */
UENUM(BlueprintType)
enum class EClimbPathInterpolation : uint8
{
    StraightSegments,
    CurvedSegments
};

UENUM(BlueprintType)
enum class EClimbSectionProfile : uint8
{
    Default,
    LedgeTraverse,
    VerticalClimb,
    HorizontalTraverse
};

UENUM(BlueprintType)
enum class EClimbTraversalType : uint8
{
    ClimbTransition,
    Jump,
    Drop,
    Mantle,
    SurfaceTransfer
};

/**
 * A route node's contact frame. Local X points outward from the surface,
 * local Z is the preferred up direction, and local Y completes the frame.
 *
 * AnchorTransform is relative to SurfaceComponent when bAnchorRelativeToSurface
 * is true. Otherwise it is relative to the owning route actor.
 */
USTRUCT(BlueprintType)
struct CLIMBSYSTEM_API FClimbRouteNode
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
    FGuid NodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Node")
    EClimbRouteNodeType Type = EClimbRouteNodeType::Standard;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Anchor")
    FTransform AnchorTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Anchor")
    TObjectPtr<UPrimitiveComponent> SurfaceComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Anchor")
    bool bAnchorRelativeToSurface = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Anchor",
        meta=(ClampMin="0.0"))
    float SurfaceOffset = 5.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Node")
    bool bCanStartTraversal = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Node")
    bool bCanEndTraversal = true;
};

/**
 * The editable geometry for a section. IntermediatePoints are stored in
 * route-actor local space. The graph-node anchors provide the two endpoints.
 * This deliberately does not depend on USplineComponent.
 */
USTRUCT(BlueprintType)
struct CLIMBSYSTEM_API FClimbPathDefinition
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Path")
    EClimbPathInterpolation Interpolation = EClimbPathInterpolation::StraightSegments;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Path")
    TArray<FVector> IntermediatePoints;
};

/** A continuous path between two meaningful graph nodes. */
USTRUCT(BlueprintType)
struct CLIMBSYSTEM_API FClimbRouteSection
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
    FGuid SectionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connections")
    FGuid StartNodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connections")
    FGuid EndNodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Path")
    FClimbPathDefinition Path;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement")
    EClimbSectionProfile Profile = EClimbSectionProfile::Default;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement")
    bool bBidirectional = true;
};

/** Type-specific jump values kept compact for the initial authoring pass. */
USTRUCT(BlueprintType)
struct CLIMBSYSTEM_API FClimbJumpSettings
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jump",
        meta=(ClampMin="0.01"))
    float Duration = 0.8f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jump",
        meta=(ClampMin="0.0"))
    float ArcHeight = 80.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jump")
    bool bUseAuthoredTrajectory = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Jump")
    TObjectPtr<UAnimSequence> Animation = nullptr;
};

/** An explicit action connecting two route nodes, separate from continuous paths. */
USTRUCT(BlueprintType)
struct CLIMBSYSTEM_API FClimbRouteTraversalLink
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
    FGuid LinkId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connections")
    FGuid FromNodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connections")
    FGuid ToNodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traversal")
    EClimbTraversalType Type = EClimbTraversalType::Jump;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traversal")
    bool bBidirectional = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traversal",
        meta=(ClampMin="0.01"))
    float Duration = 0.8f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traversal")
    float ArcHeight = 80.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traversal")
    TObjectPtr<UAnimSequence> Animation = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Landing")
    bool bOverrideLandingPose = false;

    /** Route-actor local transform used when the link overrides its destination pose. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Landing")
    FTransform LandingRootTransform = FTransform::Identity;
};
