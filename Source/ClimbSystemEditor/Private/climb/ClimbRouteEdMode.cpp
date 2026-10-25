// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbRouteEdMode.h"

#include "climb/ClimbRouteActor.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "EditorViewportClient.h"
#include "HitProxies.h"
#include "SceneManagement.h"
#include "ScopedTransaction.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "ClimbRouteEdMode"

namespace
{
    enum class ERouteHandleAction : uint8
    {
        SelectNode,
        Section,
        Jump,
        Mantle,
        Exit
    };

    struct HClimbRouteHandleProxy : public HHitProxy
    {
        DECLARE_HIT_PROXY();

        HClimbRouteHandleProxy(const FGuid& InNodeId, ERouteHandleAction InAction)
            : HHitProxy(HPP_World)
            , NodeId(InNodeId)
            , Action(InAction)
        {
        }

        FGuid NodeId;
        ERouteHandleAction Action;
    };

    IMPLEMENT_HIT_PROXY(HClimbRouteHandleProxy, HHitProxy);
}

const FEditorModeID FClimbRouteEdMode::EM_ClimbRoute = TEXT("EM_ClimbRoute");

FClimbRouteEdMode::FClimbRouteEdMode()
{
}

void FClimbRouteEdMode::Enter()
{
    FEdMode::Enter();
    SourceNodeId.Invalidate();
    PlacementIntent = EPlacementIntent::None;
    ReportModeState();
}

void FClimbRouteEdMode::Exit()
{
    PlacementIntent = EPlacementIntent::None;
    SourceNodeId.Invalidate();
    FEdMode::Exit();
}

bool FClimbRouteEdMode::InputKey(
    FEditorViewportClient* ViewportClient,
    FViewport* Viewport,
    const FKey Key,
    const EInputEvent Event)
{
    if (Event != IE_Pressed)
    {
        return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
    }

    if (Key == EKeys::Escape)
    {
        PlacementIntent = EPlacementIntent::None;
        ReportModeState();
        return true;
    }

    // Keyboard shortcuts remain available for quick authoring.
    if (Key == EKeys::N)
    {
        PlacementIntent = PlacementIntent == EPlacementIntent::IsolatedNode
            ? EPlacementIntent::None : EPlacementIntent::IsolatedNode;
        ReportModeState();
        return true;
    }

    if (Key == EKeys::P)
    {
        PlacementIntent = PlacementIntent == EPlacementIntent::Section
            ? EPlacementIntent::None : EPlacementIntent::Section;
        ReportModeState();
        return true;
    }

    if (Key == EKeys::J || Key == EKeys::D || Key == EKeys::M)
    {
        const EPlacementIntent RequestedIntent = Key == EKeys::J
            ? EPlacementIntent::Jump
            : (Key == EKeys::D ? EPlacementIntent::Drop : EPlacementIntent::Mantle);

        PlacementIntent = PlacementIntent == RequestedIntent
            ? EPlacementIntent::None : RequestedIntent;
        ReportModeState();
        return true;
    }

    return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

bool FClimbRouteEdMode::HandleClick(
    FEditorViewportClient* InViewportClient,
    HHitProxy* HitProxy,
    const FViewportClick& Click)
{
    AClimbRouteActor* Route = GetSelectedRoute();
    if (!IsValid(Route))
    {
        return false;
    }

    if (HClimbRouteHandleProxy* HandleProxy = HitProxyCast<HClimbRouteHandleProxy>(HitProxy))
    {
        SourceNodeId = HandleProxy->NodeId;
        switch (HandleProxy->Action)
        {
        case ERouteHandleAction::Section:
            PlacementIntent = EPlacementIntent::Section;
            break;
        case ERouteHandleAction::Jump:
            PlacementIntent = EPlacementIntent::Jump;
            break;
        case ERouteHandleAction::Mantle:
            PlacementIntent = EPlacementIntent::Mantle;
            break;
        case ERouteHandleAction::Exit:
            PlacementIntent = EPlacementIntent::Exit;
            break;
        default:
            PlacementIntent = EPlacementIntent::None;
            break;
        }
        ReportModeState();
        return true;
    }

    FHitResult Hit;
    if (!TraceSurface(Click, Hit))
    {
        return false;
    }

    if (PlacementIntent == EPlacementIntent::None)
    {
        SelectNearestNode(Route, Hit.ImpactPoint);
        return true;
    }

    if (PlacementIntent == EPlacementIntent::Exit && !IsValidExitSurface(Hit))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Climb Route: exit points must be placed on an upward-facing, near-horizontal surface."));
        return true;
    }

    if (PlacementIntent == EPlacementIntent::Exit && !SourceNodeId.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Climb Route: select a source node before placing an exit."));
        return true;
    }

    const FTransform ContactFrame = MakeContactFrame(Hit);
    const FText TransactionLabel = PlacementIntent == EPlacementIntent::Section
        ? LOCTEXT("AddClimbSection", "Add Climbing Path Section")
        : (PlacementIntent == EPlacementIntent::Exit
            ? LOCTEXT("AddClimbExit", "Add Climbing Exit")
            : LOCTEXT("AddClimbTraversal", "Add Climbing Traversal"));

    const FScopedTransaction Transaction(TransactionLabel);
    Route->Modify();

    EClimbRouteNodeType NewNodeType = EClimbRouteNodeType::Standard;
    if (PlacementIntent == EPlacementIntent::Exit)
    {
        NewNodeType = EClimbRouteNodeType::Exit;
    }
    else if (PlacementIntent == EPlacementIntent::Jump
        || PlacementIntent == EPlacementIntent::Drop
        || PlacementIntent == EPlacementIntent::Mantle)
    {
        NewNodeType = EClimbRouteNodeType::TraversalAnchor;
    }

    const FGuid PreviousSourceNodeId = SourceNodeId;
    const FGuid NewNodeId = Route->AddNodeAtWorldTransform(
        ContactFrame,
        Cast<UPrimitiveComponent>(Hit.GetComponent()),
        5.0f,
        NewNodeType);

    if (!NewNodeId.IsValid())
    {
        return true;
    }

    if (PreviousSourceNodeId.IsValid())
    {
        if (PlacementIntent == EPlacementIntent::Section)
        {
            // Sections are bidirectional by default, so the same authored path
            // supports both climbing up and descending back down.
            Route->AddSectionBetweenNodes(PreviousSourceNodeId, NewNodeId);
        }
        else if (PlacementIntent == EPlacementIntent::Jump
            || PlacementIntent == EPlacementIntent::Drop
            || PlacementIntent == EPlacementIntent::Mantle
            || PlacementIntent == EPlacementIntent::Exit)
        {
            const EClimbTraversalType TraversalType =
                PlacementIntent == EPlacementIntent::Jump
                ? EClimbTraversalType::Jump
                : (PlacementIntent == EPlacementIntent::Drop
                    ? EClimbTraversalType::Drop
                    : EClimbTraversalType::Mantle);
            Route->AddTraversalLink(PreviousSourceNodeId, NewNodeId, TraversalType);
        }
    }

    SourceNodeId = NewNodeId;
    if (PlacementIntent == EPlacementIntent::Exit)
    {
        PlacementIntent = EPlacementIntent::None;
    }
    ReportModeState();
    return true;
}

void FClimbRouteEdMode::Render(
    const FSceneView* View,
    FViewport* Viewport,
    FPrimitiveDrawInterface* PDI)
{
    FEdMode::Render(View, Viewport, PDI);
    if (AClimbRouteActor* Route = GetSelectedRoute())
    {
        DrawRoute(Route, View, Viewport, PDI);
    }
}

AClimbRouteActor* FClimbRouteEdMode::GetSelectedRoute() const
{
    if (!GEditor)
    {
        return nullptr;
    }

    for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
    {
        if (AClimbRouteActor* Route = Cast<AClimbRouteActor>(*It))
        {
            return Route;
        }
    }
    return nullptr;
}

bool FClimbRouteEdMode::TraceSurface(
    const FViewportClick& Click,
    FHitResult& OutHit) const
{
    if (!GEditor)
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return false;
    }

    const FVector Start = Click.GetOrigin();
    const FVector End = Start + Click.GetDirection() * 100000.0f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ClimbRouteEditorPlacement), true);
    Params.bTraceComplex = true;
    return World->LineTraceSingleByChannel(
        OutHit, Start, End, ECC_Visibility, Params);
}

bool FClimbRouteEdMode::TraceMouseSurface(
    const FSceneView* View,
    FViewport* Viewport,
    FHitResult& OutHit) const
{
    if (!GEditor || !View || !Viewport)
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return false;
    }

    FVector RayOrigin;
    FVector RayDirection;
    View->DeprojectFVector2D(
        FVector2D(Viewport->GetMouseX(), Viewport->GetMouseY()),
        RayOrigin, RayDirection);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ClimbRouteEditorPreview), true);
    Params.bTraceComplex = true;
    return World->LineTraceSingleByChannel(
        OutHit, RayOrigin, RayOrigin + RayDirection * 100000.0f,
        ECC_Visibility, Params);
}

FTransform FClimbRouteEdMode::MakeContactFrame(const FHitResult& Hit) const
{
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Normal).GetSafeNormal();
    if (Up.IsNearlyZero())
    {
        Up = FVector::VectorPlaneProject(FVector::ForwardVector, Normal).GetSafeNormal();
    }
    if (Up.IsNearlyZero())
    {
        Up = FVector::VectorPlaneProject(FVector::RightVector, Normal).GetSafeNormal();
    }

    return FTransform(FRotationMatrix::MakeFromXZ(Normal, Up).ToQuat(), Hit.ImpactPoint);
}

bool FClimbRouteEdMode::IsValidExitSurface(const FHitResult& Hit) const
{
    // Only accept a walkable top surface. Side walls and undersides are rejected.
    return Hit.ImpactNormal.GetSafeNormal().Z >= 0.65f;
}

void FClimbRouteEdMode::SelectNearestNode(
    AClimbRouteActor* Route,
    const FVector& WorldPoint)
{
    SourceNodeId.Invalidate();
    double BestDistanceSquared = FMath::Square(90.0);

    for (const FClimbRouteNode& Node : Route->Nodes)
    {
        FTransform NodeTransform;
        if (!Route->GetNodeWorldTransform(Node.NodeId, NodeTransform))
        {
            continue;
        }

        const double DistanceSquared = FVector::DistSquared(
            WorldPoint, NodeTransform.GetLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            SourceNodeId = Node.NodeId;
        }
    }

    if (SourceNodeId.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("Climb Route: source node selected: %s"),
            *SourceNodeId.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Climb Route: no node selected. Click near a node marker."));
    }
    ReportModeState();
}

void FClimbRouteEdMode::DrawRoute(
    AClimbRouteActor* Route,
    const FSceneView* View,
    FViewport* Viewport,
    FPrimitiveDrawInterface* PDI) const
{
    if (!IsValid(Route))
    {
        return;
    }

    for (const FClimbRouteSection& Section : Route->Sections)
    {
        TArray<FVector> Points;
        if (Route->GetSectionPolylineWorldPoints(Section, Points))
        {
            for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
            {
                PDI->DrawLine(Points[Index], Points[Index + 1],
                    FLinearColor(0.12f, 0.65f, 1.0f), SDPG_World, 3.0f);
            }
        }
    }

    for (const FClimbRouteTraversalLink& Link : Route->TraversalLinks)
    {
        FTransform FromTransform;
        FTransform ToTransform;
        if (!Route->GetNodeWorldTransform(Link.FromNodeId, FromTransform)
            || !Route->GetNodeWorldTransform(Link.ToNodeId, ToTransform))
        {
            continue;
        }

        const FLinearColor LinkColor = Link.Type == EClimbTraversalType::Jump
            ? FLinearColor(1.0f, 0.55f, 0.1f)
            : FLinearColor(0.25f, 0.85f, 0.35f);
        PDI->DrawLine(FromTransform.GetLocation(), ToTransform.GetLocation(),
            LinkColor, SDPG_World, 2.0f);
    }

    for (const FClimbRouteNode& Node : Route->Nodes)
    {
        FTransform NodeTransform;
        if (!Route->GetNodeWorldTransform(Node.NodeId, NodeTransform))
        {
            continue;
        }

        const FVector Origin = NodeTransform.GetLocation();
        const FVector AxisX = NodeTransform.GetRotation().GetAxisX();
        const FVector AxisY = NodeTransform.GetRotation().GetAxisY();
        const FVector AxisZ = NodeTransform.GetRotation().GetAxisZ();
        const bool bIsSource = Node.NodeId == SourceNodeId;

        PDI->SetHitProxy(new HClimbRouteHandleProxy(
            Node.NodeId, ERouteHandleAction::SelectNode));
        PDI->DrawPoint(Origin, Node.Type == EClimbRouteNodeType::Exit
            ? FLinearColor(0.1f, 0.9f, 0.75f)
            : (bIsSource ? FLinearColor::Yellow : FLinearColor::White),
            bIsSource ? 16.0f : 12.0f, SDPG_Foreground);
        PDI->SetHitProxy(nullptr);

        PDI->DrawLine(Origin, Origin + AxisX * 28.0f,
            FLinearColor::Red, SDPG_World, 2.0f);

        // Show a compact radial menu only for the selected source node.
        if (!bIsSource)
        {
            continue;
        }

        struct FButtonSpec
        {
            FVector Offset;
            FLinearColor Color;
            ERouteHandleAction Action;
        };

        const FButtonSpec Buttons[] =
        {
            { -AxisY * 58.0f, FLinearColor(0.10f, 0.65f, 1.0f), ERouteHandleAction::Section },
            {  AxisZ * 58.0f, FLinearColor(0.20f, 0.9f, 0.35f), ERouteHandleAction::Mantle },
            {  AxisY * 58.0f, FLinearColor(1.0f, 0.52f, 0.08f), ERouteHandleAction::Jump },
            { -AxisZ * 58.0f, FLinearColor(0.10f, 0.85f, 0.72f), ERouteHandleAction::Exit }
        };

        const auto DrawButtonLine = [&](const FVector& Start, const FVector& End,
            const FLinearColor& Color, const ERouteHandleAction Action)
        {
            PDI->SetHitProxy(new HClimbRouteHandleProxy(Node.NodeId, Action));
            PDI->DrawLine(Start, End, Color, SDPG_Foreground, 2.5f);
            PDI->SetHitProxy(nullptr);
        };

        for (const FButtonSpec& Button : Buttons)
        {
            const FVector Center = Origin + Button.Offset;
            const float Radius = 13.0f;
            const int32 CircleSegments = 20;
            for (int32 Segment = 0; Segment < CircleSegments; ++Segment)
            {
                const float A0 = 2.0f * PI * static_cast<float>(Segment)
                    / static_cast<float>(CircleSegments);
                const float A1 = 2.0f * PI * static_cast<float>(Segment + 1)
                    / static_cast<float>(CircleSegments);
                const FVector P0 = Center + AxisY * (FMath::Cos(A0) * Radius)
                    + AxisZ * (FMath::Sin(A0) * Radius);
                const FVector P1 = Center + AxisY * (FMath::Cos(A1) * Radius)
                    + AxisZ * (FMath::Sin(A1) * Radius);
                DrawButtonLine(P0, P1, Button.Color, Button.Action);
            }

            PDI->SetHitProxy(new HClimbRouteHandleProxy(Node.NodeId, Button.Action));
            PDI->DrawPoint(Center, Button.Color, 4.0f, SDPG_Foreground);
            PDI->SetHitProxy(nullptr);

            const float IconScale = 7.0f;
            if (Button.Action == ERouteHandleAction::Section)
            {
                DrawButtonLine(Center - AxisY * IconScale * 0.65f,
                    Center + AxisY * IconScale * 0.65f, Button.Color, Button.Action);
                DrawButtonLine(Center - AxisY * IconScale * 0.65f - AxisZ * 2.5f,
                    Center - AxisY * IconScale * 0.65f + AxisZ * 2.5f,
                    Button.Color, Button.Action);
                DrawButtonLine(Center + AxisY * IconScale * 0.65f - AxisZ * 2.5f,
                    Center + AxisY * IconScale * 0.65f + AxisZ * 2.5f,
                    Button.Color, Button.Action);
            }
            else if (Button.Action == ERouteHandleAction::Mantle)
            {
                DrawButtonLine(Center - AxisZ * IconScale * 0.55f,
                    Center + AxisZ * IconScale * 0.5f, Button.Color, Button.Action);
                DrawButtonLine(Center + AxisZ * IconScale * 0.5f,
                    Center + AxisZ * IconScale * 0.1f + AxisY * IconScale * 0.38f,
                    Button.Color, Button.Action);
                DrawButtonLine(Center + AxisZ * IconScale * 0.5f,
                    Center + AxisZ * IconScale * 0.1f - AxisY * IconScale * 0.38f,
                    Button.Color, Button.Action);
            }
            else if (Button.Action == ERouteHandleAction::Jump)
            {
                const FVector Start = Center - AxisY * IconScale * 0.42f
                    - AxisZ * IconScale * 0.42f;
                const FVector Tip = Center + AxisY * IconScale * 0.42f
                    + AxisZ * IconScale * 0.42f;
                DrawButtonLine(Start, Tip, Button.Color, Button.Action);
                DrawButtonLine(Tip, Tip - AxisY * IconScale * 0.38f,
                    Button.Color, Button.Action);
                DrawButtonLine(Tip, Tip - AxisZ * IconScale * 0.38f,
                    Button.Color, Button.Action);
            }
            else if (Button.Action == ERouteHandleAction::Exit)
            {
                DrawButtonLine(Center - AxisY * IconScale * 0.55f,
                    Center + AxisY * IconScale * 0.25f, Button.Color, Button.Action);
                DrawButtonLine(Center + AxisY * IconScale * 0.25f,
                    Center + AxisY * IconScale * 0.25f - AxisZ * IconScale * 0.38f,
                    Button.Color, Button.Action);
                DrawButtonLine(Center + AxisY * IconScale * 0.25f,
                    Center + AxisY * IconScale * 0.25f + AxisZ * IconScale * 0.38f,
                    Button.Color, Button.Action);
            }
        }
    }

    // Live ghost placement: trace under the viewport cursor, draw a preview point,
    // and connect it to the selected source without committing any route data.
    if (PlacementIntent != EPlacementIntent::None && SourceNodeId.IsValid())
    {
        FHitResult PreviewHit;
        if (TraceMouseSurface(View, Viewport, PreviewHit))
        {
            const bool bExitSurfaceValid = PlacementIntent != EPlacementIntent::Exit
                || IsValidExitSurface(PreviewHit);
            const FTransform GhostFrame = MakeContactFrame(PreviewHit);
            const FVector GhostLocation = GhostFrame.GetLocation()
                + GhostFrame.GetUnitAxis(EAxis::X) * 5.0f;

            FTransform SourceTransform;
            if (Route->GetNodeWorldTransform(SourceNodeId, SourceTransform))
            {
                const FLinearColor PreviewColor = !bExitSurfaceValid
                    ? FLinearColor::Red
                    : (PlacementIntent == EPlacementIntent::Exit
                        ? FLinearColor(0.1f, 0.85f, 0.72f)
                        : (PlacementIntent == EPlacementIntent::Jump
                            ? FLinearColor(1.0f, 0.52f, 0.08f)
                            : (PlacementIntent == EPlacementIntent::Mantle
                                ? FLinearColor(0.2f, 0.9f, 0.35f)
                                : FLinearColor(0.1f, 0.65f, 1.0f))));

                PDI->DrawLine(SourceTransform.GetLocation(), GhostLocation,
                    PreviewColor, SDPG_Foreground, 2.0f);
                PDI->DrawPoint(GhostLocation, PreviewColor, 15.0f, SDPG_Foreground);
                PDI->DrawLine(GhostLocation,
                    GhostLocation + GhostFrame.GetUnitAxis(EAxis::X) * 24.0f,
                    PreviewColor, SDPG_Foreground, 1.5f);
            }
        }
    }
}

void FClimbRouteEdMode::ReportModeState() const
{
    const TCHAR* IntentName = TEXT("Select a node to reveal radial actions");
    switch (PlacementIntent)
    {
    case EPlacementIntent::IsolatedNode: IntentName = TEXT("Place isolated node"); break;
    case EPlacementIntent::Section: IntentName = TEXT("Place bidirectional climb section"); break;
    case EPlacementIntent::Jump: IntentName = TEXT("Place jump destination"); break;
    case EPlacementIntent::Drop: IntentName = TEXT("Place explicit drop destination"); break;
    case EPlacementIntent::Mantle: IntentName = TEXT("Place mantle destination"); break;
    case EPlacementIntent::Exit: IntentName = TEXT("Place exit on horizontal top surface"); break;
    default: break;
    }

    UE_LOG(LogTemp, Display,
        TEXT("Climb Route Editor | %s | Source node: %s | N=node, P=section, J=jump, D=drop, M=mantle, Esc=cancel"),
        IntentName,
        SourceNodeId.IsValid() ? *SourceNodeId.ToString() : TEXT("none"));
}

#undef LOCTEXT_NAMESPACE
