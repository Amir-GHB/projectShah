// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbRouteActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AClimbRouteActor::AClimbRouteActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    RuntimePathVisualization = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
        TEXT("RuntimePathVisualization"));
    RuntimePathVisualization->SetupAttachment(SceneRoot);
    RuntimePathVisualization->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RuntimePathVisualization->SetGenerateOverlapEvents(false);
    RuntimePathVisualization->SetCanEverAffectNavigation(false);
    RuntimePathVisualization->SetCastShadow(false);
    RuntimePathVisualization->SetMobility(EComponentMobility::Movable);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        RuntimePathVisualization->SetStaticMesh(CylinderMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        UMaterialInstanceDynamic* DynamicMaterial =
            UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetVectorParameterValue(TEXT("Color"), RuntimePathColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), RuntimePathColor);
            RuntimePathVisualization->SetMaterial(0, DynamicMaterial);
        }
    }
}

void AClimbRouteActor::BeginPlay()
{
    Super::BeginPlay();
    BuildRuntimePathVisualization();
}

void AClimbRouteActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
}

void AClimbRouteActor::BuildRuntimePathVisualization()
{
    if (!RuntimePathVisualization)
    {
        return;
    }

    RuntimePathVisualization->ClearInstances();
    if (!bShowRuntimePathVisualization || !RuntimePathVisualization->GetStaticMesh())
    {
        RuntimePathVisualization->SetVisibility(false, true);
        return;
    }

    RuntimePathVisualization->SetVisibility(true, true);
    if (UMaterialInstanceDynamic* DynamicMaterial =
        Cast<UMaterialInstanceDynamic>(RuntimePathVisualization->GetMaterial(0)))
    {
        DynamicMaterial->SetVectorParameterValue(TEXT("Color"), RuntimePathColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), RuntimePathColor);
        DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), RuntimePathColor.A);
    }

    for (const FClimbRouteSection& Section : Sections)
    {
        TArray<FVector> Points;
        if (!GetSectionPolylineWorldPoints(Section, Points) || Points.Num() < 2)
        {
            continue;
        }

        for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
        {
            FVector Start = Points[Index];
            FVector End = Points[Index + 1];
            const FVector Direction = (End - Start).GetSafeNormal();
            if (Direction.IsNearlyZero())
            {
                continue;
            }

            if (Index == 0)
            {
                Start += Direction * RuntimePathNodeGap;
            }
            if (Index == Points.Num() - 2)
            {
                End -= Direction * RuntimePathNodeGap;
            }
            AddRuntimePathLine(Start, End, RuntimePathThickness);
        }
    }

    for (const FClimbRouteTraversalLink& Link : TraversalLinks)
    {
        FTransform FromTransform;
        FTransform ToTransform;
        if (!GetNodeWorldTransform(Link.FromNodeId, FromTransform)
            || !GetNodeWorldTransform(Link.ToNodeId, ToTransform))
        {
            continue;
        }

        FVector Start = FromTransform.GetLocation();
        FVector End = ToTransform.GetLocation();
        const FVector Direction = (End - Start).GetSafeNormal();
        if (Direction.IsNearlyZero())
        {
            continue;
        }

        Start += Direction * RuntimePathNodeGap;
        End -= Direction * RuntimePathNodeGap;
        AddRuntimePathLine(Start, End, RuntimePathThickness);
    }

    // Short world-vertical ticks make node positions readable without touching
    // the gameplay collision or relying on development-only debug drawing.
    for (const FClimbRouteNode& Node : Nodes)
    {
        FTransform NodeTransform;
        if (!GetNodeWorldTransform(Node.NodeId, NodeTransform))
        {
            continue;
        }

        const FVector Origin = NodeTransform.GetLocation();
        const FVector HalfTick = FVector::UpVector * (RuntimeNodeMarkerLength * 0.5f);
        AddRuntimePathLine(Origin - HalfTick, Origin + HalfTick,
            RuntimePathThickness * 0.85f);
    }
}

void AClimbRouteActor::AddRuntimePathLine(
    const FVector& Start,
    const FVector& End,
    const float Radius)
{
    if (!RuntimePathVisualization)
    {
        return;
    }

    const FVector Delta = End - Start;
    const float Length = Delta.Size();
    if (Length <= KINDA_SMALL_NUMBER || Length < RuntimePathNodeGap * 0.5f)
    {
        return;
    }

    const FQuat Rotation = FQuat::FindBetweenNormals(
        FVector::UpVector, Delta / Length);
    const FVector Scale(
        FMath::Max(Radius, 0.25f) / 50.0f,
        FMath::Max(Radius, 0.25f) / 50.0f,
        Length / 100.0f);
    RuntimePathVisualization->AddInstance(
        FTransform(Rotation, (Start + End) * 0.5f, Scale), true);
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
