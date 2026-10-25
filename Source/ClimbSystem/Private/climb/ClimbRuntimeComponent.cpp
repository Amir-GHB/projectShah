// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbRuntimeComponent.h"

#include "climb/ClimbRouteActor.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UClimbRuntimeComponent::UClimbRuntimeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UClimbRuntimeComponent::BeginPlay()
{
    Super::BeginPlay();
    RefreshAvailableRoutes();
}

void UClimbRuntimeComponent::RefreshAvailableRoutes()
{
    AvailableRoutes.Reset();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AClimbRouteActor> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            AvailableRoutes.Add(*It);
        }
    }
}

bool UClimbRuntimeComponent::StartClimbing()
{
    if (IsClimbing())
    {
        return true;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!IsValid(Character) || !Character->GetCharacterMovement())
    {
        return false;
    }

    AClimbRouteActor* Route = nullptr;
    FGuid NodeId;
    FTransform NodeTransform;
    if (!FindClosestAttachNode(Route, NodeId, NodeTransform))
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("Climbing: no authored entry node within %.1f units."), MaxAttachDistance);
        return false;
    }

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    PreviousMovementMode = Movement->MovementMode;
    Movement->StopMovementImmediately();
    Movement->SetMovementMode(MOVE_None);

    FVector TargetLocation;
    FRotator TargetRotation;
    if (!GetCharacterLocationAtNode(Route, NodeId, TargetLocation, TargetRotation))
    {
        Movement->SetMovementMode(PreviousMovementMode.GetValue());
        return false;
    }

    CurrentRoute = Route;
    CurrentNodeId = NodeId;
    TargetRoute.Reset();
    TargetNodeId.Invalidate();
    ClimbInput = FVector2D::ZeroVector;
    RuntimeState = EClimbRuntimeState::Attached;

    Character->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr,
        ETeleportType::TeleportPhysics);

    UE_LOG(LogTemp, Log, TEXT("Climbing: attached to route '%s', node %s."),
        *GetNameSafe(Route), *NodeId.ToString());
    return true;
}

void UClimbRuntimeComponent::StopClimbing()
{
    if (!IsClimbing())
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (IsValid(Character))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            EMovementMode RestoreMode = PreviousMovementMode.GetValue();
            if (RestoreMode == MOVE_None)
            {
                RestoreMode = MOVE_Walking;
            }
            Movement->SetMovementMode(RestoreMode);
        }
    }

    RuntimeState = EClimbRuntimeState::Inactive;
    CurrentRoute.Reset();
    CurrentNodeId.Invalidate();
    TargetRoute.Reset();
    TargetNodeId.Invalidate();
    ClimbInput = FVector2D::ZeroVector;
    ActiveTraversalElapsed = 0.0f;
    ActiveTraversalDuration = 0.0f;
}

void UClimbRuntimeComponent::SetClimbInput(const FVector2D Input)
{
    ClimbInput = Input.GetClampedToMaxSize(1.0f);
}

bool UClimbRuntimeComponent::FindClosestAttachNode(
    AClimbRouteActor*& OutRoute,
    FGuid& OutNodeId,
    FTransform& OutNodeTransform) const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!IsValid(Character))
    {
        return false;
    }

    float BestDistanceSquared = FMath::Square(MaxAttachDistance);
    bool bFoundNode = false;

    for (const TWeakObjectPtr<AClimbRouteActor>& RoutePtr : AvailableRoutes)
    {
        AClimbRouteActor* Route = RoutePtr.Get();
        if (!IsValid(Route))
        {
            continue;
        }

        for (const FClimbRouteNode& Node : Route->Nodes)
        {
            if (!Node.bCanStartTraversal)
            {
                continue;
            }

            FTransform NodeTransform;
            FVector CandidateLocation;
            FRotator CandidateRotation;
            if (!Route->GetNodeWorldTransform(Node.NodeId, NodeTransform)
                || !GetCharacterLocationAtNode(Route, Node.NodeId,
                    CandidateLocation, CandidateRotation))
            {
                continue;
            }

            const float DistanceSquared = FVector::DistSquared(
                Character->GetActorLocation(), CandidateLocation);
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                OutRoute = Route;
                OutNodeId = Node.NodeId;
                OutNodeTransform = NodeTransform;
                bFoundNode = true;
            }
        }
    }

    return bFoundNode;
}

bool UClimbRuntimeComponent::GetCharacterLocationAtNode(
    AClimbRouteActor* Route,
    const FGuid& NodeId,
    FVector& OutLocation,
    FRotator& OutRotation) const
{
    if (!IsValid(Route))
    {
        return false;
    }

    FTransform NodeTransform;
    if (!Route->GetNodeWorldTransform(NodeId, NodeTransform))
    {
        return false;
    }

    float CapsuleRadius = 42.0f;
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
        {
            CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        }
    }

    const FVector OutwardNormal = NodeTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
    OutLocation = NodeTransform.GetLocation()
        + OutwardNormal * (CapsuleRadius + AttachmentClearance);
    OutRotation = GetClimbingRotation(NodeTransform);
    return true;
}

FVector UClimbRuntimeComponent::GetSurfaceUp(const FTransform& NodeTransform) const
{
    const FVector OutwardNormal = NodeTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
    FVector Up = FVector::VectorPlaneProject(
        NodeTransform.GetUnitAxis(EAxis::Z), OutwardNormal).GetSafeNormal();

    if (Up.IsNearlyZero())
    {
        Up = FVector::VectorPlaneProject(FVector::UpVector, OutwardNormal).GetSafeNormal();
    }
    if (Up.IsNearlyZero())
    {
        Up = FVector::VectorPlaneProject(FVector::RightVector, OutwardNormal).GetSafeNormal();
    }
    return Up;
}

FRotator UClimbRuntimeComponent::GetClimbingRotation(const FTransform& NodeTransform) const
{
    const FVector OutwardNormal = NodeTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
    const FVector Forward = -OutwardNormal;
    FVector Up = GetSurfaceUp(NodeTransform);

    if (Forward.IsNearlyZero())
    {
        return GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
    }
    if (Up.IsNearlyZero())
    {
        Up = FVector::UpVector;
    }

    return FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
}

bool UClimbRuntimeComponent::FindBestConnection(FRuntimeConnection& OutConnection) const
{
    AClimbRouteActor* Route = CurrentRoute.Get();
    if (!IsValid(Route) || !CurrentNodeId.IsValid() || ClimbInput.IsNearlyZero())
    {
        return false;
    }

    FTransform CurrentTransform;
    if (!Route->GetNodeWorldTransform(CurrentNodeId, CurrentTransform))
    {
        return false;
    }

    const FVector OutwardNormal = CurrentTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
    const FVector SurfaceUp = GetSurfaceUp(CurrentTransform);
    const FVector Forward = -OutwardNormal;
    const FVector SurfaceRight = FVector::CrossProduct(SurfaceUp, Forward).GetSafeNormal();
    FVector DesiredDirection = SurfaceUp * ClimbInput.Y + SurfaceRight * ClimbInput.X;
    DesiredDirection = FVector::VectorPlaneProject(DesiredDirection, OutwardNormal).GetSafeNormal();

    if (DesiredDirection.IsNearlyZero())
    {
        return false;
    }

    float BestScore = InputDirectionThreshold;
    bool bFound = false;

    const auto ConsiderCandidate = [&](AClimbRouteActor* CandidateRoute,
        const FGuid& CandidateNodeId, const EClimbTraversalType Type,
        const bool bIsTraversalLink, const float Duration, const float ArcHeight)
    {
        if (!IsValid(CandidateRoute) || !CandidateNodeId.IsValid()
            || (CandidateRoute == Route && CandidateNodeId == CurrentNodeId))
        {
            return;
        }

        const FClimbRouteNode* CandidateNode = CandidateRoute->FindNode(CandidateNodeId);
        if (!CandidateNode || !CandidateNode->bCanEndTraversal)
        {
            return;
        }

        FTransform CandidateTransform;
        if (!CandidateRoute->GetNodeWorldTransform(CandidateNodeId, CandidateTransform))
        {
            return;
        }

        const FVector Direction = FVector::VectorPlaneProject(
            CandidateTransform.GetLocation() - CurrentTransform.GetLocation(),
            OutwardNormal).GetSafeNormal();
        if (Direction.IsNearlyZero())
        {
            return;
        }

        const float Score = FVector::DotProduct(DesiredDirection, Direction);
        if (Score > BestScore)
        {
            BestScore = Score;
            OutConnection.Route = CandidateRoute;
            OutConnection.TargetNodeId = CandidateNodeId;
            OutConnection.Type = Type;
            OutConnection.bIsTraversalLink = bIsTraversalLink;
            OutConnection.Duration = Duration;
            OutConnection.ArcHeight = ArcHeight;
            bFound = true;
        }
    };

    for (const FClimbRouteSection& Section : Route->Sections)
    {
        if (Section.StartNodeId == CurrentNodeId)
        {
            ConsiderCandidate(Route, Section.EndNodeId,
                EClimbTraversalType::ClimbTransition, false, 0.0f, 0.0f);
        }
        if (Section.bBidirectional && Section.EndNodeId == CurrentNodeId)
        {
            ConsiderCandidate(Route, Section.StartNodeId,
                EClimbTraversalType::ClimbTransition, false, 0.0f, 0.0f);
        }
    }

    for (const FClimbRouteTraversalLink& Link : Route->TraversalLinks)
    {
        if (Link.FromNodeId == CurrentNodeId)
        {
            ConsiderCandidate(Route, Link.ToNodeId, Link.Type, true,
                FMath::Max(Link.Duration, 0.05f), Link.ArcHeight);
        }
        if (Link.bBidirectional && Link.ToNodeId == CurrentNodeId)
        {
            ConsiderCandidate(Route, Link.FromNodeId, Link.Type, true,
                FMath::Max(Link.Duration, 0.05f), Link.ArcHeight);
        }
    }

    return bFound;
}

void UClimbRuntimeComponent::BeginConnection(const FRuntimeConnection& Connection)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    AClimbRouteActor* DestinationRoute = Connection.Route.Get();
    if (!IsValid(Character) || !IsValid(DestinationRoute))
    {
        return;
    }

    FVector DestinationLocation;
    FRotator DestinationRotation;
    if (!GetCharacterLocationAtNode(DestinationRoute, Connection.TargetNodeId,
        DestinationLocation, DestinationRotation))
    {
        return;
    }

    TraversalStartLocation = Character->GetActorLocation();
    TraversalEndLocation = DestinationLocation;
    TraversalStartRotation = Character->GetActorRotation();
    TraversalEndRotation = DestinationRotation;
    TargetRoute = DestinationRoute;
    TargetNodeId = Connection.TargetNodeId;
    ActiveTraversalType = Connection.Type;
    bActiveConnectionIsTraversalLink = Connection.bIsTraversalLink;
    ActiveTraversalArcHeight = Connection.ArcHeight;

    ActiveTraversalDuration = Connection.bIsTraversalLink
        ? FMath::Max(Connection.Duration, 0.05f)
        : FMath::Max(FVector::Distance(TraversalStartLocation, TraversalEndLocation)
            / FMath::Max(ClimbSpeed, 1.0f), 0.05f);

    ActiveTraversalElapsed = 0.0f;
    RuntimeState = Connection.bIsTraversalLink
        ? EClimbRuntimeState::ExecutingTraversal
        : EClimbRuntimeState::MovingAlongSection;
}

void UClimbRuntimeComponent::FinishConnection()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    AClimbRouteActor* DestinationRoute = TargetRoute.Get();

    if (!IsValid(Character) || !IsValid(DestinationRoute)
        || !DestinationRoute->FindNode(TargetNodeId))
    {
        StopClimbing();
        return;
    }

    FVector DestinationLocation;
    FRotator DestinationRotation;
    if (GetCharacterLocationAtNode(DestinationRoute, TargetNodeId,
        DestinationLocation, DestinationRotation))
    {
        Character->SetActorLocationAndRotation(DestinationLocation,
            DestinationRotation, false, nullptr, ETeleportType::TeleportPhysics);
    }

    CurrentRoute = DestinationRoute;
    CurrentNodeId = TargetNodeId;
    TargetRoute.Reset();
    TargetNodeId.Invalidate();
    ActiveTraversalElapsed = 0.0f;
    ActiveTraversalDuration = 0.0f;
    ActiveTraversalArcHeight = 0.0f;
    RuntimeState = EClimbRuntimeState::Attached;
}

void UClimbRuntimeComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsClimbing())
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!IsValid(Character) || !CurrentRoute.IsValid())
    {
        StopClimbing();
        return;
    }

    if (RuntimeState == EClimbRuntimeState::Attached)
    {
        if (ClimbInput.IsNearlyZero())
        {
            return;
        }

        FRuntimeConnection Connection;
        if (FindBestConnection(Connection))
        {
            BeginConnection(Connection);
        }
        return;
    }

    if (RuntimeState != EClimbRuntimeState::MovingAlongSection
        && RuntimeState != EClimbRuntimeState::ExecutingTraversal)
    {
        return;
    }

    if (!TargetRoute.IsValid() || !TargetNodeId.IsValid())
    {
        StopClimbing();
        return;
    }

    ActiveTraversalElapsed += DeltaTime;
    const float Alpha = FMath::Clamp(
        ActiveTraversalElapsed / FMath::Max(ActiveTraversalDuration, 0.05f), 0.0f, 1.0f);

    FVector Location = FMath::Lerp(TraversalStartLocation, TraversalEndLocation, Alpha);
    if (bActiveConnectionIsTraversalLink
        && (ActiveTraversalType == EClimbTraversalType::Jump
            || ActiveTraversalType == EClimbTraversalType::Mantle))
    {
        Location.Z += FMath::Sin(Alpha * PI) * ActiveTraversalArcHeight;
    }

    const FQuat Rotation = FQuat::Slerp(
        TraversalStartRotation.Quaternion(),
        TraversalEndRotation.Quaternion(),
        Alpha);

    Character->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
        ETeleportType::TeleportPhysics);

    if (bDrawRuntimeDebug)
    {
        DrawDebugLine(GetWorld(), TraversalStartLocation, TraversalEndLocation,
            RuntimeState == EClimbRuntimeState::ExecutingTraversal
                ? FColor::Orange : FColor::Cyan,
            false, 0.0f, 0, 2.0f);
    }

    if (Alpha >= 1.0f)
    {
        FinishConnection();
    }
}