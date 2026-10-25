// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "climb/ClimbRouteTypes.h"
#include "ClimbRouteActor.generated.h"

class UPrimitiveComponent;
class USceneComponent;

/**
 * Owns one connected climbing authoring network.
 *
 * This is the new authoring actor. The legacy AClimbRoute remains available
 * during migration so existing Blueprint references are not broken abruptly.
 */
UCLASS(Blueprintable)
class CLIMBSYSTEM_API AClimbRouteActor : public AActor
{
    GENERATED_BODY()

public:
    AClimbRouteActor();

    /** Create a node from a surface contact frame in world space. */
    FGuid AddNodeAtWorldTransform(
        const FTransform& ContactFrame,
        UPrimitiveComponent* SurfaceComponent,
        float InSurfaceOffset = 5.0f,
        EClimbRouteNodeType NodeType = EClimbRouteNodeType::Standard);

    bool AddSectionBetweenNodes(const FGuid& StartNodeId, const FGuid& EndNodeId);
    bool AddTraversalLink(
        const FGuid& FromNodeId,
        const FGuid& ToNodeId,
        EClimbTraversalType Type);

    bool RemoveNode(const FGuid& NodeId);
    const FClimbRouteNode* FindNode(const FGuid& NodeId) const;
    bool GetNodeWorldTransform(const FGuid& NodeId, FTransform& OutTransform) const;
    bool GetSectionPolylineWorldPoints(const FClimbRouteSection& Section,
        TArray<FVector>& OutPoints) const;

    /** Returns false when structural errors are found. */
    bool ValidateRoute(TArray<FString>& OutErrors) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing Route")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing Route|Authoring")
    TArray<FClimbRouteNode> Nodes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing Route|Authoring")
    TArray<FClimbRouteSection> Sections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing Route|Authoring")
    TArray<FClimbRouteTraversalLink> TraversalLinks;

#if WITH_EDITORONLY_DATA
    UPROPERTY(Transient)
    bool bShowAuthoringVisualization = true;
#endif

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    int32 FindNodeIndex(const FGuid& NodeId) const;
};
