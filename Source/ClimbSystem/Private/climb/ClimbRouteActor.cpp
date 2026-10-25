// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbRouteActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"

AClimbRouteActor::AClimbRouteActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
}

void AClimbRouteActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
}

FGuid AClimbRouteActor::AddNodeAtWorldTransform(
    const FTransform& ContactFrame,
    UPrimitiveComponent* InSurfaceComponent,
    const float InSurfaceOffset,
    const EClimbRouteNodeType NodeType)
{
    Modify();

    FClimbRouteNode NewNode;
    NewNode.NodeId = FGuid::NewGuid();
    NewNode.Type = NodeType;
    NewNode.SurfaceComponent = InSurfaceComponent;
    NewNode.SurfaceOffset = FMath::Max(0.0f, InSurfaceOffset);

    const FTransform ParentTransform = IsValid(InSurfaceComponent)
        ? InSurfaceComponent->GetComponentTransform()
        : GetActorTransform();

    NewNode.bAnchorRelativeToSurface = IsValid(InSurfaceComponent);
    NewNode.AnchorTransform = ContactFrame.GetRelativeTransform(ParentTransform);

    Nodes.Add(NewNode);
    MarkPackageDirty();
    return NewNode.NodeId;
}

bool AClimbRouteActor::AddSectionBetweenNodes(
    const FGuid& StartNodeId,
    const FGuid& EndNodeId)
{
    if (!StartNodeId.IsValid() || !EndNodeId.IsValid()
        || StartNodeId == EndNodeId
        || !FindNode(StartNodeId) || !FindNode(EndNodeId))
    {
        return false;
    }

    for (const FClimbRouteSection& Existing : Sections)
    {
        if (Existing.StartNodeId == StartNodeId && Existing.EndNodeId == EndNodeId)
        {
            return false;
        }
    }

    Modify();

    FClimbRouteSection NewSection;
    NewSection.SectionId = FGuid::NewGuid();
    NewSection.StartNodeId = StartNodeId;
    NewSection.EndNodeId = EndNodeId;
    Sections.Add(NewSection);

    MarkPackageDirty();
    return true;
}

bool AClimbRouteActor::AddTraversalLink(
    const FGuid& FromNodeId,
    const FGuid& ToNodeId,
    const EClimbTraversalType Type)
{
    if (!FromNodeId.IsValid() || !ToNodeId.IsValid()
        || FromNodeId == ToNodeId
        || !FindNode(FromNodeId) || !FindNode(ToNodeId))
    {
        return false;
    }

    Modify();

    FClimbRouteTraversalLink NewLink;
    NewLink.LinkId = FGuid::NewGuid();
    NewLink.FromNodeId = FromNodeId;
    NewLink.ToNodeId = ToNodeId;
    NewLink.Type = Type;
    TraversalLinks.Add(NewLink);

    MarkPackageDirty();
    return true;
}

bool AClimbRouteActor::RemoveNode(const FGuid& NodeId)
{
    const int32 NodeIndex = FindNodeIndex(NodeId);
    if (NodeIndex == INDEX_NONE)
    {
        return false;
    }

    Modify();
    Nodes.RemoveAt(NodeIndex);

    Sections.RemoveAll([&NodeId](const FClimbRouteSection& Section)
    {
        return Section.StartNodeId == NodeId || Section.EndNodeId == NodeId;
    });

    TraversalLinks.RemoveAll([&NodeId](const FClimbRouteTraversalLink& Link)
    {
        return Link.FromNodeId == NodeId || Link.ToNodeId == NodeId;
    });

    MarkPackageDirty();
    return true;
}

const FClimbRouteNode* AClimbRouteActor::FindNode(const FGuid& NodeId) const
{
    const int32 Index = FindNodeIndex(NodeId);
    return Nodes.IsValidIndex(Index) ? &Nodes[Index] : nullptr;
}

bool AClimbRouteActor::GetNodeWorldTransform(
    const FGuid& NodeId,
    FTransform& OutTransform) const
{
    const FClimbRouteNode* Node = FindNode(NodeId);
    if (!Node)
    {
        return false;
    }

    const UPrimitiveComponent* Surface = Node->SurfaceComponent.Get();
    const FTransform ParentTransform =
        Node->bAnchorRelativeToSurface && IsValid(Surface)
        ? Surface->GetComponentTransform()
        : GetActorTransform();

    OutTransform = Node->AnchorTransform * ParentTransform;
    const FVector Offset = OutTransform.GetRotation().RotateVector(
        FVector(Node->SurfaceOffset, 0.0f, 0.0f));
    OutTransform.SetLocation(OutTransform.GetLocation() + Offset);
    return true;
}

bool AClimbRouteActor::GetSectionPolylineWorldPoints(
    const FClimbRouteSection& Section,
    TArray<FVector>& OutPoints) const
{
    OutPoints.Reset();

    FTransform StartTransform;
    FTransform EndTransform;
    if (!GetNodeWorldTransform(Section.StartNodeId, StartTransform)
        || !GetNodeWorldTransform(Section.EndNodeId, EndTransform))
    {
        return false;
    }

    OutPoints.Add(StartTransform.GetLocation());
    for (const FVector& LocalPoint : Section.Path.IntermediatePoints)
    {
        OutPoints.Add(GetActorTransform().TransformPosition(LocalPoint));
    }
    OutPoints.Add(EndTransform.GetLocation());
    return true;
}

bool AClimbRouteActor::ValidateRoute(TArray<FString>& OutErrors) const
{
    OutErrors.Reset();
    TSet<FGuid> NodeIds;
    TSet<FGuid> SectionIds;
    TSet<FGuid> LinkIds;

    for (const FClimbRouteNode& Node : Nodes)
    {
        if (!Node.NodeId.IsValid())
        {
            OutErrors.Add(TEXT("A route node has an invalid GUID."));
        }
        else if (NodeIds.Contains(Node.NodeId))
        {
            OutErrors.Add(FString::Printf(TEXT("Duplicate node GUID: %s"),
                *Node.NodeId.ToString()));
        }
        NodeIds.Add(Node.NodeId);

        if (Node.SurfaceOffset < 0.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("Node %s has a negative surface offset."),
                *Node.NodeId.ToString()));
        }
    }

    for (const FClimbRouteSection& Section : Sections)
    {
        if (!Section.SectionId.IsValid() || SectionIds.Contains(Section.SectionId))
        {
            OutErrors.Add(TEXT("A section has an invalid or duplicate GUID."));
        }
        SectionIds.Add(Section.SectionId);

        if (!NodeIds.Contains(Section.StartNodeId) || !NodeIds.Contains(Section.EndNodeId))
        {
            OutErrors.Add(FString::Printf(TEXT("Section %s references a missing node."),
                *Section.SectionId.ToString()));
        }
        if (Section.StartNodeId == Section.EndNodeId)
        {
            OutErrors.Add(FString::Printf(TEXT("Section %s connects a node to itself."),
                *Section.SectionId.ToString()));
        }
    }

    for (const FClimbRouteTraversalLink& Link : TraversalLinks)
    {
        if (!Link.LinkId.IsValid() || LinkIds.Contains(Link.LinkId))
        {
            OutErrors.Add(TEXT("A traversal link has an invalid or duplicate GUID."));
        }
        LinkIds.Add(Link.LinkId);

        if (!NodeIds.Contains(Link.FromNodeId) || !NodeIds.Contains(Link.ToNodeId))
        {
            OutErrors.Add(FString::Printf(TEXT("Traversal link %s references a missing node."),
                *Link.LinkId.ToString()));
        }
        if (Link.FromNodeId == Link.ToNodeId)
        {
            OutErrors.Add(FString::Printf(TEXT("Traversal link %s connects a node to itself."),
                *Link.LinkId.ToString()));
        }
        if (Link.Duration <= 0.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("Traversal link %s has a non-positive duration."),
                *Link.LinkId.ToString()));
        }
    }

    return OutErrors.IsEmpty();
}

int32 AClimbRouteActor::FindNodeIndex(const FGuid& NodeId) const
{
    return Nodes.IndexOfByPredicate([&NodeId](const FClimbRouteNode& Node)
    {
        return Node.NodeId == NodeId;
    });
}
