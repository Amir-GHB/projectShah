// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "climb/ClimbRouteTypes.h"
#include "ClimbRouteActor.generated.h"

class UPrimitiveComponent;
class USceneComponent;
class UInstancedStaticMeshComponent;

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

    /** Runtime path dressing, rendered as ordinary instanced scene geometry (not debug lines). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing Route|Visualization")
    TObjectPtr<UInstancedStaticMeshComponent> RuntimePathVisualization = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing Route|Visualization")
    bool bShowRuntimePathVisualization = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing Route|Visualization")
    FLinearColor RuntimePathColor = FLinearColor(0.34f, 0.23f, 0.055f, 0.72f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing Route|Visualization",
        meta=(ClampMin="0.25", ClampMax="8.0"))
    float RuntimePathThickness = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing Route|Visualization",
        meta=(ClampMin="0.0", ClampMax="50.0"))
    float RuntimePathNodeGap = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing Route|Visualization",
        meta=(ClampMin="1.0", ClampMax="50.0"))
    float RuntimeNodeMarkerLength = 16.0f;

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
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    int32 FindNodeIndex(const FGuid& NodeId) const;
    void BuildRuntimePathVisualization();
    void AddRuntimePathLine(const FVector& Start, const FVector& End, float Radius);
};
