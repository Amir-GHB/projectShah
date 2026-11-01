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
 * Select a node to reveal its radial action buttons. Blue creates a bidirectional
 * climbing section, green creates a mantle, orange creates a jump, and teal
 * creates a horizontal exit node. A ghost point previews the new anchor.
 * Escape cancels placement.
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
        Mantle,
        Exit
    };

    AClimbRouteActor* GetSelectedRoute() const;
    bool TraceSurface(const FViewportClick& Click, FHitResult& OutHit) const;
    bool TraceMouseSurface(const FSceneView* View, FViewport* Viewport,
        FHitResult& OutHit) const;
    FTransform MakeContactFrame(const FHitResult& Hit) const;
    bool IsValidExitSurface(const FHitResult& Hit) const;
    void SelectNearestNode(AClimbRouteActor* Route, const FVector& WorldPoint);
    void DrawRoute(AClimbRouteActor* Route, const FSceneView* View,
        FViewport* Viewport, FPrimitiveDrawInterface* PDI) const;
    void ReportModeState() const;

    FGuid SourceNodeId;
    EPlacementIntent PlacementIntent = EPlacementIntent::None;
};
