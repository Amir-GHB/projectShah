// Copyright Epic Games, Inc. All Rights Reserved.

#include "climb/ClimbRuntimeComponent.h"

#include "climb/ClimbRouteActor.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RotationMatrix.h"
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
            TEXT("Climbing: no usable authored node within %.1f units."), MaxAttachDistance);
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

    SetAnimationState(EClimbMovementType::Enter,
        TargetLocation - Character->GetActorLocation(),
        Character->GetActorRightVector());

    Character->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr,
        ETeleportType::TeleportPhysics);
    SetRuntimeState(EClimbRuntimeState::Attached);

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

    SetAnimationState(EClimbMovementType::Leave, FVector::ZeroVector,
        Character ? Character->GetActorRightVector() : FVector::RightVector);
    SetRuntimeState(EClimbRuntimeState::Inactive);
    bActiveDropCatch = false;
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
            if (!Node.bCanStartTraversal || Node.Type == EClimbRouteNodeType::Exit)
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

    const FClimbRouteNode* Node = Route->FindNode(NodeId);
    if (!Node)
    {
        return false;
    }

    float CapsuleRadius = 42.0f;
    float CapsuleHalfHeight = 96.0f;
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
        {
            CapsuleRadius = Capsule->GetScaledCapsuleRadius();
            CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        }
    }

    const FVector OutwardNormal = NodeTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
    if (Node->Type == EClimbRouteNodeType::Exit)
    {
        // Exit nodes are authored on upward-facing, approximately horizontal surfaces.
        // GetNodeWorldTransform already includes SurfaceOffset, so recover the surface
        // contact point before placing the capsule upright on top of it.
        const FVector SurfacePoint = NodeTransform.GetLocation()
            - OutwardNormal * Node->SurfaceOffset;
        OutLocation = SurfacePoint + FVector::UpVector * CapsuleHalfHeight;

        FVector Forward = FVector::VectorPlaneProject(
            NodeTransform.GetUnitAxis(EAxis::Z), FVector::UpVector).GetSafeNormal();
        if (Forward.IsNearlyZero())
        {
            if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
            {
                Forward = FVector::VectorPlaneProject(
                    Character->GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
            }
        }
        if (Forward.IsNearlyZero())
        {
            Forward = FVector::ForwardVector;
        }
        OutRotation = FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).Rotator();
        return true;
    }

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

        const FVector Delta = CandidateTransform.GetLocation()
            - CurrentTransform.GetLocation();
        const FVector Direction = FVector::VectorPlaneProject(
            Delta, OutwardNormal).GetSafeNormal();
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
            const bool bDescending = FVector::DotProduct(
                Delta.GetSafeNormal(), SurfaceUp) < -0.55f;
            OutConnection.bDropCatch = Type == EClimbTraversalType::Drop
                || (!bIsTraversalLink && bDescending);
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
    bActiveDropCatch = Connection.bDropCatch;
    ActiveTraversalArcHeight = Connection.ArcHeight;

    FTransform SourceTransform;
    FTransform DestinationTransform;
    if (CurrentRoute.IsValid())
    {
        CurrentRoute->GetNodeWorldTransform(CurrentNodeId, SourceTransform);
    }
    DestinationRoute->GetNodeWorldTransform(Connection.TargetNodeId, DestinationTransform);
    const FClimbRouteNode* DestinationNode = DestinationRoute->FindNode(Connection.TargetNodeId);
    const EClimbMovementType MovementType =
        DestinationNode && DestinationNode->Type == EClimbRouteNodeType::Exit
            ? EClimbMovementType::Exit
            : (Connection.Type == EClimbTraversalType::Jump
                ? EClimbMovementType::Jump
                : (Connection.Type == EClimbTraversalType::Mantle
                    ? EClimbMovementType::Mantle
                    : EClimbMovementType::Smooth));
    const FVector WorldDisplacement = TraversalEndLocation - TraversalStartLocation;
    AnimationWorldRightAxis = Character->GetActorRightVector();
    SetAnimationState(MovementType, WorldDisplacement, AnimationWorldRightAxis);

    const float Distance = FVector::Distance(TraversalStartLocation, TraversalEndLocation);
    if (bActiveDropCatch)
    {
        ActiveTraversalDuration = FMath::Clamp(
            FMath::Sqrt(FMath::Max(Distance, 1.0f) / 980.0f) * 2.0f,
            0.28f, 0.75f);
    }
    else
    {
        ActiveTraversalDuration = Connection.bIsTraversalLink
            ? FMath::Max(Connection.Duration, 0.05f)
            : FMath::Max(Distance / FMath::Max(ClimbSpeed, 1.0f), 0.05f);
    }

    ActiveTraversalElapsed = 0.0f;
    SetRuntimeState(bActiveDropCatch
        ? EClimbRuntimeState::DroppingToCatch
        : (Connection.bIsTraversalLink
            ? EClimbRuntimeState::ExecutingTraversal
            : EClimbRuntimeState::MovingAlongSection));
}

void UClimbRuntimeComponent::FinishConnection()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    AClimbRouteActor* DestinationRoute = TargetRoute.Get();
    const FClimbRouteNode* DestinationNode = IsValid(DestinationRoute)
        ? DestinationRoute->FindNode(TargetNodeId) : nullptr;

    if (!IsValid(Character) || !IsValid(DestinationRoute) || !DestinationNode)
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

    if (DestinationNode->Type == EClimbRouteNodeType::Exit)
    {
        // Reaching an exit node returns control to ordinary character movement.
        StopClimbing();
        return;
    }

    CurrentRoute = DestinationRoute;
    CurrentNodeId = TargetNodeId;
    TargetRoute.Reset();
    TargetNodeId.Invalidate();
    ActiveTraversalElapsed = 0.0f;
    ActiveTraversalDuration = 0.0f;
    ActiveTraversalArcHeight = 0.0f;
    bActiveDropCatch = false;
    ClearAnimationState();
    SetRuntimeState(EClimbRuntimeState::Attached);
}

void UClimbRuntimeComponent::SetAnimationState(
    const EClimbMovementType MovementType,
    const FVector& WorldDisplacement,
    const FVector& WorldRightAxis)
{
    AnimationState.MovementType = MovementType;
    AnimationWorldRightAxis = WorldRightAxis.GetSafeNormal();
    if (AnimationWorldRightAxis.IsNearlyZero())
    {
        AnimationWorldRightAxis = FVector::RightVector;
    }

    const FVector Direction3D = WorldDisplacement.GetSafeNormal();
    AnimationState.Direction = FVector2D(
        FVector::DotProduct(Direction3D, AnimationWorldRightAxis),
        FVector::DotProduct(Direction3D, FVector::UpVector));
    AnimationState.Direction = AnimationState.Direction.GetClampedToMaxSize(1.0f);

    AnimationStateRemainingTime =
        (MovementType == EClimbMovementType::Enter
            || MovementType == EClimbMovementType::Leave)
        ? AnimationTransitionHoldTime : 0.0f;
}

void UClimbRuntimeComponent::ClearAnimationState()
{
    AnimationState.MovementType = EClimbMovementType::None;
    AnimationState.Direction = FVector2D::ZeroVector;
    AnimationStateRemainingTime = 0.0f;
}

void UClimbRuntimeComponent::SetRuntimeState(const EClimbRuntimeState NewState)
{
    if (RuntimeState == NewState)
    {
        return;
    }

    const EClimbRuntimeState PreviousState = RuntimeState;
    RuntimeState = NewState;
    OnRuntimeStateChanged.Broadcast(PreviousState, NewState);
}

void UClimbRuntimeComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (AnimationStateRemainingTime > 0.0f)
    {
        AnimationStateRemainingTime = FMath::Max(
            0.0f, AnimationStateRemainingTime - DeltaTime);
        if (AnimationStateRemainingTime <= 0.0f)
        {
            ClearAnimationState();
        }
    }

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
        && RuntimeState != EClimbRuntimeState::ExecutingTraversal
        && RuntimeState != EClimbRuntimeState::DroppingToCatch)
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
    float MovementAlpha = Alpha;
    if (bActiveDropCatch)
    {
        // Brief release, accelerating fall, then a short eased catch at the target.
        if (Alpha < 0.12f)
        {
            MovementAlpha = 0.02f * (Alpha / 0.12f);
        }
        else if (Alpha < 0.78f)
        {
            const float FallAlpha = (Alpha - 0.12f) / 0.66f;
            MovementAlpha = 0.02f + 0.88f * FallAlpha * FallAlpha;
        }
        else
        {
            const float CatchAlpha = (Alpha - 0.78f) / 0.22f;
            MovementAlpha = 0.90f + 0.10f * (1.0f - FMath::Square(1.0f - CatchAlpha));
        }
    }

    FVector Location = FMath::Lerp(
        TraversalStartLocation, TraversalEndLocation, MovementAlpha);
    if (bActiveDropCatch)
    {
        FTransform SourceTransform;
        if (CurrentRoute.IsValid()
            && CurrentRoute->GetNodeWorldTransform(CurrentNodeId, SourceTransform))
        {
            const FVector OutwardNormal =
                SourceTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
            // The body briefly peels away from the wall before accelerating down
            // and easing into the catch pose at the lower authored node.
            Location += OutwardNormal * FMath::Sin(Alpha * PI)
                * DropCatchOutwardDistance;
        }
    }
    else if (bActiveConnectionIsTraversalLink
        && (ActiveTraversalType == EClimbTraversalType::Jump
            || ActiveTraversalType == EClimbTraversalType::Mantle))
    {
        Location.Z += FMath::Sin(Alpha * PI) * ActiveTraversalArcHeight;
    }

    const FQuat Rotation = FQuat::Slerp(
        TraversalStartRotation.Quaternion(),
        TraversalEndRotation.Quaternion(),
        bActiveDropCatch ? FMath::Clamp(Alpha * 1.2f, 0.0f, 1.0f) : Alpha);

    Character->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
        ETeleportType::TeleportPhysics);

    if (bDrawRuntimeDebug)
    {
        DrawDebugLine(GetWorld(), TraversalStartLocation, TraversalEndLocation,
            RuntimeState == EClimbRuntimeState::ExecutingTraversal
                ? FColor::Orange
                : (RuntimeState == EClimbRuntimeState::DroppingToCatch
                    ? FColor::Yellow : FColor::Cyan),
            false, 0.0f, 0, 2.0f);
    }

    if (Alpha >= 1.0f)
    {
        FinishConnection();
    }
}