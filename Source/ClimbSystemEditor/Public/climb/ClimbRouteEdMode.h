// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"
#include "climb/ClimbRouteTypes.h"

class AClimbRouteActor;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class FEditorViewportClient;
class HHitProxy;

/**
 * First-pass viewport authoring mode.
 *
 * Activate the "Climb Route" editor mode with an AClimbRouteActor selected.
 * Click a node to choose it as the source. P creates a straight section,
 * N places an isolated node, J creates a jump, D a drop, and M a mantle.
 * Escape cancels the current placement operation.
 */
class CLIMBSYSTEMEDITOR_API FClimbRouteEdMode : public FEdMode
{
public:
    static const FEditorModeID EM_ClimbRoute;

    FClimbRouteEdMode();

    virtual void Enter() override;
    virtual void Exit() override;
    virtual bool UsesToolkits() const override { return false; }

    virtual bool InputKey(
        FEditorViewportClient* ViewportClient,
        FViewport* Viewport,
        FKey Key,
        EInputEvent Event) override;

    virtual bool HandleClick(
        FEditorViewportClient* InViewportClient,
        HHitProxy* HitProxy,
        const FViewportClick& Click) override;

    virtual void Render(
        const FSceneView* View,
        FViewport* Viewport,
        FPrimitiveDrawInterface* PDI) override;

private:
    enum class EPlacementIntent : uint8
    {
        None,
        IsolatedNode,
        Section,
        Jump,
        Drop,
        Mantle
    };

    AClimbRouteActor* GetSelectedRoute() const;
    bool TraceSurface(const FViewportClick& Click, FHitResult& OutHit) const;
    void SelectNearestNode(AClimbRouteActor* Route, const FVector& WorldPoint);
    void DrawRoute(AClimbRouteActor* Route, FPrimitiveDrawInterface* PDI) const;
    void ReportModeState() const;

    FGuid SourceNodeId;
    EPlacementIntent PlacementIntent = EPlacementIntent::None;
};
