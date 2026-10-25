// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbRouteEdMode.h"

#include "climb/ClimbRouteActor.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "EditorViewportClient.h"
#include "SceneManagement.h"
#include "ScopedTransaction.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "ClimbRouteEdMode"

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

    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    if (Normal.IsNearlyZero())
    {
        return true;
    }

    FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Normal).GetSafeNormal();
    if (Up.IsNearlyZero())
    {
        Up = FVector::VectorPlaneProject(FVector::ForwardVector, Normal).GetSafeNormal();
    }
    if (Up.IsNearlyZero())
    {
        Up = FVector::VectorPlaneProject(FVector::RightVector, Normal).GetSafeNormal();
    }

    const FQuat ContactRotation = FRotationMatrix::MakeFromXZ(Normal, Up).ToQuat();
    const FTransform ContactFrame(ContactRotation, Hit.ImpactPoint);

    const FText TransactionLabel = PlacementIntent == EPlacementIntent::Section
        ? LOCTEXT("AddClimbSection", "Add Climbing Path Section")
        : LOCTEXT("AddClimbTraversal", "Add Climbing Traversal");

    const FScopedTransaction Transaction(TransactionLabel);
    Route->Modify();

    const EClimbRouteNodeType NewNodeType =
        PlacementIntent == EPlacementIntent::Jump
        || PlacementIntent == EPlacementIntent::Drop
        || PlacementIntent == EPlacementIntent::Mantle
        ? EClimbRouteNodeType::TraversalAnchor
        : EClimbRouteNodeType::Standard;

    const FGuid NewNodeId = Route->AddNodeAtWorldTransform(
        ContactFrame,
        Cast<UPrimitiveComponent>(Hit.GetComponent()),
        5.0f,
        NewNodeType);

    if (!NewNodeId.IsValid())
    {
        return true;
    }

    if (SourceNodeId.IsValid())
    {
        if (PlacementIntent == EPlacementIntent::Section)
        {
            Route->AddSectionBetweenNodes(SourceNodeId, NewNodeId);
        }
        else if (PlacementIntent == EPlacementIntent::Jump
            || PlacementIntent == EPlacementIntent::Drop
            || PlacementIntent == EPlacementIntent::Mantle)
        {
            const EClimbTraversalType TraversalType =
                PlacementIntent == EPlacementIntent::Jump
                ? EClimbTraversalType::Jump
                : (PlacementIntent == EPlacementIntent::Drop
                    ? EClimbTraversalType::Drop
                    : EClimbTraversalType::Mantle);
            Route->AddTraversalLink(SourceNodeId, NewNodeId, TraversalType);
        }
    }

    SourceNodeId = NewNodeId;
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
        DrawRoute(Route, PDI);
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
            : FLinearColor(0.85f, 0.3f, 0.9f);
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

        const bool bIsSource = Node.NodeId == SourceNodeId;
        const FLinearColor NodeColor = bIsSource
            ? FLinearColor::Yellow
            : (Node.Type == EClimbRouteNodeType::Entry
                ? FLinearColor::Green : FLinearColor::White);
        PDI->DrawPoint(NodeTransform.GetLocation(), NodeColor,
            bIsSource ? 16.0f : 10.0f, SDPG_World);
        PDI->DrawLine(
            NodeTransform.GetLocation(),
            NodeTransform.GetLocation() + NodeTransform.GetRotation().GetAxisX() * 28.0f,
            FLinearColor::Red, SDPG_World, 2.0f);
    }
}

void FClimbRouteEdMode::ReportModeState() const
{
    const TCHAR* IntentName = TEXT("Select node");
    switch (PlacementIntent)
    {
    case EPlacementIntent::IsolatedNode: IntentName = TEXT("Place node"); break;
    case EPlacementIntent::Section: IntentName = TEXT("Place section endpoint (straight path)"); break;
    case EPlacementIntent::Jump: IntentName = TEXT("Place jump destination"); break;
    case EPlacementIntent::Drop: IntentName = TEXT("Place drop destination"); break;
    case EPlacementIntent::Mantle: IntentName = TEXT("Place mantle destination"); break;
    default: break;
    }

    UE_LOG(LogTemp, Display,
        TEXT("Climb Route Editor | %s | Source node: %s | N=node, P=section, J=jump, D=drop, M=mantle, Esc=cancel"),
        IntentName,
        SourceNodeId.IsValid() ? *SourceNodeId.ToString() : TEXT("none"));
}

#undef LOCTEXT_NAMESPACE
