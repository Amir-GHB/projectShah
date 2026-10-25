// Fill out your copyright notice in the Description page of Project Settings.



#include "Public/climb/ClimbingComponent.h"
#include "Public/climb/ClimbRoute.h"

#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UClimbingComponent::UClimbingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UClimbingComponent::BeginPlay()
{
    Super::BeginPlay();

    BuildRouteGraph();
}

void UClimbingComponent::BuildRouteGraph()
{
    GeneratedPoints.Reset();

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    for (TActorIterator<AClimbRoute> It(World); It; ++It)
    {
        AClimbRoute* Route = *It;

        if (!IsValid(Route))
        {
            continue;
        }

        TArray<USplineComponent*> Splines;
        Route->GetComponents<USplineComponent>(Splines);

        for (USplineComponent* Spline : Splines)
        {
            if (!IsValid(Spline))
            {
                continue;
            }

            const float Length = Spline->GetSplineLength();

            if (Length <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            const float Spacing = FMath::Max(Route->PointSpacing, 5.0f);

            const int32 SegmentCount = FMath::Max(
                1,
                FMath::CeilToInt(Length / Spacing)
            );

            for (int32 Sample = 0; Sample <= SegmentCount; ++Sample)
            {
                const float Alpha =
                    static_cast<float>(Sample) / SegmentCount;

                const float Distance = Length * Alpha;

                const FVector SplineLocation =
                    Spline->GetLocationAtDistanceAlongSpline(
                        Distance,
                        ESplineCoordinateSpace::World
                    );

                const FVector AuthoredNormal =
                    Spline->GetUpVectorAtDistanceAlongSpline(
                        Distance,
                        ESplineCoordinateSpace::World
                    ).GetSafeNormal();

                if (AuthoredNormal.IsNearlyZero())
                {
                    continue;
                }

                // Project the spline sample onto the climbing surface.
                const FVector TraceStart =
                    SplineLocation +
                    AuthoredNormal * Route->ProjectionDistance;

                const FVector TraceEnd =
                    SplineLocation -
                    AuthoredNormal * Route->ProjectionDistance;

                FCollisionQueryParams Params(
                    SCENE_QUERY_STAT(ClimbRouteProjection),
                    true
                );

                Params.AddIgnoredActor(Route);

                if (AActor* Owner = GetOwner())
                {
                    Params.AddIgnoredActor(Owner);
                }

                FHitResult Hit;
                if (Sample == 0 || Sample == SegmentCount)
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT(
                            "Endpoint %d/%d | Location=%s | Up=%s | TraceStart=%s | TraceEnd=%s | Channel=%d"
                        ),
                        Sample,
                        SegmentCount,
                        *SplineLocation.ToString(),
                        *AuthoredNormal.ToString(),
                        *TraceStart.ToString(),
                        *TraceEnd.ToString(),
                        static_cast<int32>(Route->ProjectionTraceChannel)
                    );

                    DrawDebugLine(
                        World,
                        TraceStart,
                        TraceEnd,
                        FColor::Red,
                        false,
                        15.0f,
                        0,
                        3.0f
                    );

                    DrawDebugSphere(
                        World,
                        SplineLocation,
                        10.0f,
                        8,
                        FColor::Magenta,
                        false,
                        15.0f
                    );
                }
                bool bHit = World->LineTraceSingleByChannel(
                    Hit,
                    TraceStart,
                    TraceEnd,
                    Route->ProjectionTraceChannel,
                    Params
                );

                // Retry in the opposite direction if necessary.
                if (!bHit)
                {
                    bHit = World->LineTraceSingleByChannel(
                        Hit,
                        TraceEnd,
                        TraceStart,
                        Route->ProjectionTraceChannel,
                        Params
                    );
                }

                if (!bHit)
                {
                    if (Sample == 0 || Sample == SegmentCount)
                    {
                        UE_LOG(
                            LogTemp,
                            Warning,
                            TEXT("Endpoint projection FAILED: Sample=%d/%d"),
                            Sample,
                            SegmentCount
                        );
                    }

                    continue;
                }

                FClimbGraphPoint Point;

                Point.SurfacePoint = Hit.ImpactPoint;
                Point.SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
                Point.Route = Route;
                Point.SourceSpline = Spline;
                Point.SampleIndex = Sample;
                Point.SampleCount = SegmentCount;
                Point.DistanceAlongSpline = Distance;

                Point.bIsEntryPoint =
                 (Sample == 0 && Route->bStartIsEntry) ||
                 (Sample == SegmentCount && Route->bEndIsEntry);
                
                if (Sample == 0 || Sample == SegmentCount)
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("Route=%s Spline=%s Sample=%d/%d StartEntry=%d EndEntry=%d IsEntry=%d"),
                        *GetNameSafe(Route),
                        *GetNameSafe(Spline),
                        Sample,
                        SegmentCount,
                        Route->bStartIsEntry,
                        Route->bEndIsEntry,
                        Point.bIsEntryPoint
                    );
                }
                
                GeneratedPoints.Add(MoveTemp(Point));
                int32 EntryCount = 0;

                for (const FClimbGraphPoint& GeneratedPoint : GeneratedPoints)
                {
                    if (GeneratedPoint.bIsEntryPoint)
                    {
                        ++EntryCount;
                    }
                }

                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("Climbing: current entry count = %d"),
                    EntryCount
                );
                
            }
        }
    }

    ConnectGraphPoints();

    BuildNavigationPoints();
    ConnectNavigationPoints();

    if (bDrawDebugGraph)
    {
        DrawGraphDebug();
        DrawNavigationDebug();
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Climbing: generated %d surface points and %d navigation points."),
        GeneratedPoints.Num(),
        NavigationPoints.Num()
    );
}

void UClimbingComponent::ConnectGraphPoints()
{
    // Connect consecutive samples on the same spline.
    // Connect nearby samples on different splines belonging
    // to the same route to form junctions.

    for (int32 A = 0; A < GeneratedPoints.Num(); ++A)
    {
        FClimbGraphPoint& PointA = GeneratedPoints[A];

        for (int32 B = A + 1; B < GeneratedPoints.Num(); ++B)
        {
            FClimbGraphPoint& PointB = GeneratedPoints[B];

            if (PointA.Route != PointB.Route)
            {
                continue;
            }

            bool bConnect = false;

            if (PointA.SourceSpline == PointB.SourceSpline)
            {
                bConnect =
                    FMath::Abs(
                        PointA.SampleIndex - PointB.SampleIndex
                    ) == 1;
            }
            else
            {
                const float DistanceSquared = FVector::DistSquared(
                    PointA.SurfacePoint,
                    PointB.SurfacePoint
                );

                const float Radius = PointA.Route->JunctionRadius;

                const bool bCloseEnough =
                    DistanceSquared <= FMath::Square(Radius);

                const bool bSimilarSurface =
                    FVector::DotProduct(
                        PointA.SurfaceNormal,
                        PointB.SurfaceNormal
                    ) >= 0.5f;

                bConnect = bCloseEnough && bSimilarSurface;
            }

            if (bConnect)
            {
                PointA.Neighbors.AddUnique(B);
                PointB.Neighbors.AddUnique(A);
            }
        }
    }
}

void UClimbingComponent::DrawGraphDebug() const
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    for (int32 Index = 0; Index < GeneratedPoints.Num(); ++Index)
    {
        const FClimbGraphPoint& Point = GeneratedPoints[Index];

        const FVector DrawLocation =
            Point.SurfacePoint + Point.SurfaceNormal * 5.0f;

        const FColor PointColor =
            Point.bIsEntryPoint ? FColor::Green : FColor::Yellow;

        DrawDebugSphere(
            World,
            DrawLocation,
            7.0f,
            8,
            PointColor,
            false,
            DebugDuration
        );

        for (const int32 NeighborIndex : Point.Neighbors)
        {
            // Draw every edge only once.
            if (NeighborIndex <= Index ||
                !GeneratedPoints.IsValidIndex(NeighborIndex))
            {
                continue;
            }

            const FClimbGraphPoint& Neighbor =
                GeneratedPoints[NeighborIndex];

            DrawDebugLine(
                World,
                DrawLocation,
                Neighbor.SurfacePoint +
                    Neighbor.SurfaceNormal * 5.0f,
                FColor::Cyan,
                false,
                DebugDuration,
                0,
                2.0f
            );
        }
    }
}

bool UClimbingComponent::StartClimbing()
{
    if (bIsClimbing)
    {
        return true;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());

    if (!Character || NavigationPoints.IsEmpty())
    {
        return false;
    }

    float BestDistanceSquared = FMath::Square(MaxAttachDistance);
    int32 BestIndex = INDEX_NONE;

    for (int32 Index = 0; Index < NavigationPoints.Num(); ++Index)
    {
        const FClimbNavigationPoint& Point = NavigationPoints[Index];

        if (!Point.bIsEntryPoint)
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            Character->GetActorLocation(), Point.SurfacePoint);

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestIndex = Index;
        }
    }

    if (BestIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Climbing: no navigation entry point within range."));
        return false;
    }

    UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement();

    if (!Movement)
    {
        return false;
    }

    PreviousMovementMode = Movement->MovementMode;

    Movement->StopMovementImmediately();
    Movement->SetMovementMode(MOVE_None);

    CurrentPointIndex = BestIndex;
    TargetPointIndex = INDEX_NONE;
    ClimbInput = FVector2D::ZeroVector;
    bIsClimbing = true;

    Character->SetActorLocation(
        GetCharacterLocationForNavigationPoint(CurrentPointIndex),
        false);

    Character->SetActorRotation(GetClimbingRotation(
        NavigationPoints[CurrentPointIndex].SurfaceNormal));

    return true;
}

void UClimbingComponent::StopClimbing()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());

    if (!Character || !bIsClimbing)
    {
        return;
    }

    if (UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement())
    {
        EMovementMode RestoreMode = PreviousMovementMode.GetValue();

        if (RestoreMode == MOVE_None)
        {
            RestoreMode = MOVE_Walking;
        }

        Movement->SetMovementMode(RestoreMode);
    }

    bIsClimbing = false;
    CurrentPointIndex = INDEX_NONE;
    TargetPointIndex = INDEX_NONE;
    ClimbInput = FVector2D::ZeroVector;
}

FVector UClimbingComponent::GetCharacterLocationForPoint(
    int32 PointIndex
) const
{
    if (!GeneratedPoints.IsValidIndex(PointIndex))
    {
        return FVector::ZeroVector;
    }

    const FClimbGraphPoint& Point = GeneratedPoints[PointIndex];

    float CapsuleRadius = 42.0f;

    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (const UCapsuleComponent* Capsule =
            Character->GetCapsuleComponent())
        {
            CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        }
    }

    return Point.SurfacePoint +
        Point.SurfaceNormal * (CapsuleRadius + SurfaceOffset);
}

FRotator UClimbingComponent::GetClimbingRotation(
    const FVector& SurfaceNormal
) const
{
   const FVector Forward = -SurfaceNormal.GetSafeNormal();

    if (Forward.IsNearlyZero())
    {
        return GetOwner()
            ? GetOwner()->GetActorRotation()
            : FRotator::ZeroRotator;
    }

    // Project world up onto the plane perpendicular to the
    // surface normal to produce a stable orientation.
    FVector Up = FVector::VectorPlaneProject(
        FVector::UpVector,
        Forward
    ).GetSafeNormal();

    // Fallback for surfaces whose normal is almost vertical.
    if (Up.IsNearlyZero())
    {
        Up = FVector::VectorPlaneProject(
            FVector::RightVector,
            Forward
        ).GetSafeNormal();
    }

    return FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
}

void UClimbingComponent::SetClimbInput(FVector2D Input)
{
    ClimbInput = Input.GetClampedToMaxSize(1.0f);
}

int32 UClimbingComponent::FindBestNeighbor(int32 FromIndex) const
{
    if (!GeneratedPoints.IsValidIndex(FromIndex) ||
        ClimbInput.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    const FClimbGraphPoint& Current = GeneratedPoints[FromIndex];

    const FVector SurfaceNormal =
        Current.SurfaceNormal.GetSafeNormal();

    const FVector ClimbUp = FVector::VectorPlaneProject(
        FVector::UpVector,
        SurfaceNormal
    ).GetSafeNormal();

    // World-up cannot define a climbing-up direction on a
    // horizontal ceiling, so use a fallback for now.
    if (ClimbUp.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    const FVector ClimbRight = FVector::CrossProduct(
        ClimbUp,
        SurfaceNormal
    ).GetSafeNormal();

    FVector DesiredDirection =
        ClimbUp * ClimbInput.Y +
        ClimbRight * ClimbInput.X;

    DesiredDirection = FVector::VectorPlaneProject(
        DesiredDirection,
        SurfaceNormal
    ).GetSafeNormal();

    if (DesiredDirection.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    float BestScore = 0.1f;
    int32 BestIndex = INDEX_NONE;

    for (const int32 NeighborIndex : Current.Neighbors)
    {
        if (!GeneratedPoints.IsValidIndex(NeighborIndex))
        {
            continue;
        }

        const FVector Direction = (
            GeneratedPoints[NeighborIndex].SurfacePoint -
            Current.SurfacePoint
        ).GetSafeNormal();

        const float Score = FVector::DotProduct(
            DesiredDirection,
            Direction
        );

        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = NeighborIndex;
        }
    }

    return BestIndex;
}

void UClimbingComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsClimbing)
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());

    if (!Character ||
        !NavigationPoints.IsValidIndex(CurrentPointIndex))
    {
        StopClimbing();
        return;
    }

    // Only choose a new destination while the player is providing input.
    // If input is released during movement, keep the existing destination.
    if (TargetPointIndex == INDEX_NONE)
    {
        if (ClimbInput.IsNearlyZero())
        {
            return;
        }

        TargetPointIndex =
            FindBestNavigationNeighbor(CurrentPointIndex);

        if (TargetPointIndex == INDEX_NONE)
        {
            return;
        }
    }

    if (!NavigationPoints.IsValidIndex(TargetPointIndex))
    {
        TargetPointIndex = INDEX_NONE;
        return;
    }

    // Continue toward the selected authored spline point,
    // even if the player has released the input.
    const FVector TargetLocation =
        GetCharacterLocationForNavigationPoint(TargetPointIndex);

    const FVector NewLocation = FMath::VInterpConstantTo(
        Character->GetActorLocation(),
        TargetLocation,
        DeltaTime,
        ClimbSpeed);

    Character->SetActorLocation(NewLocation, false);

    const FRotator TargetRotation = GetClimbingRotation(
        NavigationPoints[TargetPointIndex].SurfaceNormal);

    Character->SetActorRotation(
        FMath::RInterpTo(
            Character->GetActorRotation(),
            TargetRotation,
            DeltaTime,
            8.0f));

    // Stop only after reaching the authored navigation point.
    if (FVector::DistSquared(NewLocation, TargetLocation) < 4.0f)
    {
        Character->SetActorLocation(TargetLocation, false);

        CurrentPointIndex = TargetPointIndex;
        TargetPointIndex = INDEX_NONE;
    }
}

void UClimbingComponent::BuildNavigationPoints()
{
    NavigationPoints.Reset();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AClimbRoute> It(World); It; ++It)
    {
        AClimbRoute* Route = *It;
        if (!IsValid(Route))
        {
            continue;
        }

        TArray<USplineComponent*> Splines;
        Route->GetComponents<USplineComponent>(Splines);

        for (USplineComponent* Spline : Splines)
        {
            if (!IsValid(Spline))
            {
                continue;
            }

            const int32 NumSplinePoints = Spline->GetNumberOfSplinePoints();

            for (int32 PointIndex = 0;
                 PointIndex < NumSplinePoints;
                 ++PointIndex)
            {
                const FVector SplineLocation =
                    Spline->GetLocationAtSplinePoint(
                        PointIndex,
                        ESplineCoordinateSpace::World);

                const FVector AuthoredNormal =
                    Spline->GetUpVectorAtSplinePoint(
                        PointIndex,
                        ESplineCoordinateSpace::World).GetSafeNormal();

                if (AuthoredNormal.IsNearlyZero())
                {
                    continue;
                }

                const FVector TraceStart =
                    SplineLocation + AuthoredNormal * Route->ProjectionDistance;

                const FVector TraceEnd =
                    SplineLocation - AuthoredNormal * Route->ProjectionDistance;

                FCollisionQueryParams Params(
                    SCENE_QUERY_STAT(ClimbNavigationProjection), true);

                Params.AddIgnoredActor(Route);

                if (AActor* Owner = GetOwner())
                {
                    Params.AddIgnoredActor(Owner);
                }

                FHitResult Hit;

                bool bHit = World->LineTraceSingleByChannel(
                    Hit,
                    TraceStart,
                    TraceEnd,
                    Route->ProjectionTraceChannel,
                    Params);

                if (!bHit)
                {
                    bHit = World->LineTraceSingleByChannel(
                        Hit,
                        TraceEnd,
                        TraceStart,
                        Route->ProjectionTraceChannel,
                        Params);
                }

                if (!bHit)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("Navigation projection failed: Route=%s Point=%d"),
                        *GetNameSafe(Route), PointIndex);

                    continue;
                }

                FClimbNavigationPoint NavigationPoint;
                NavigationPoint.SurfacePoint = Hit.ImpactPoint;
                NavigationPoint.SurfaceNormal =
                    Hit.ImpactNormal.GetSafeNormal();
                NavigationPoint.Route = Route;
                NavigationPoint.SourceSpline = Spline;
                NavigationPoint.SplinePointIndex = PointIndex;

                NavigationPoint.bIsEntryPoint =
                    (PointIndex == 0 && Route->bStartIsEntry) ||
                    (PointIndex == NumSplinePoints - 1 &&
                     Route->bEndIsEntry);

                NavigationPoints.Add(MoveTemp(NavigationPoint));
            }
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("Climbing: generated %d authored navigation points."),
        NavigationPoints.Num());
}

void UClimbingComponent::ConnectNavigationPoints()
{
    for (int32 A = 0; A < NavigationPoints.Num(); ++A)
    {
        FClimbNavigationPoint& PointA = NavigationPoints[A];

        for (int32 B = A + 1; B < NavigationPoints.Num(); ++B)
        {
            FClimbNavigationPoint& PointB = NavigationPoints[B];

            if (PointA.Route != PointB.Route)
            {
                continue;
            }

            bool bConnect = false;

            // Consecutive authored points on the same spline.
            if (PointA.SourceSpline == PointB.SourceSpline)
            {
                bConnect =
                    FMath::Abs(
                        PointA.SplinePointIndex -
                        PointB.SplinePointIndex) == 1;
            }
            else
            {
                // Potential junction between two splines on the same route.
                const float DistanceSquared = FVector::DistSquared(
                    PointA.SurfacePoint,
                    PointB.SurfacePoint);

                const float Radius = PointA.Route->JunctionRadius;

                const bool bCloseEnough =
                    DistanceSquared <= FMath::Square(Radius);

                const bool bSimilarSurface =
                    FVector::DotProduct(
                        PointA.SurfaceNormal,
                        PointB.SurfaceNormal) >= 0.5f;

                bConnect = bCloseEnough && bSimilarSurface;
            }

            if (bConnect)
            {
                PointA.Neighbors.AddUnique(B);
                PointB.Neighbors.AddUnique(A);
            }
        }
    }

    // Cross-route junctions: use the dense graph to validate proximity.
    for (int32 A = 0; A < NavigationPoints.Num(); ++A)
    {
        FClimbNavigationPoint& PointA = NavigationPoints[A];

        for (int32 B = A + 1; B < NavigationPoints.Num(); ++B)
        {
            FClimbNavigationPoint& PointB = NavigationPoints[B];

            if (PointA.Route == PointB.Route)
            {
                continue;
            }

            const float Radius = FMath::Min(
                PointA.Route->JunctionRadius,
                PointB.Route->JunctionRadius);

            if (FVector::DistSquared(
                    PointA.SurfacePoint, PointB.SurfacePoint) >
                FMath::Square(Radius))
            {
                continue;
            }

            if (FVector::DotProduct(
                    PointA.SurfaceNormal,
                    PointB.SurfaceNormal) < 0.5f)
            {
                continue;
            }

            // Require nearby dense samples from both routes.
            bool bDenseConnectionExists = false;

            for (const FClimbGraphPoint& DenseA : GeneratedPoints)
            {
                if (DenseA.Route != PointA.Route ||
                    DenseA.SourceSpline != PointA.SourceSpline)
                {
                    continue;
                }

                if (FVector::DistSquared(
                        DenseA.SurfacePoint, PointA.SurfacePoint) >
                    FMath::Square(PointA.Route->PointSpacing * 1.5f))
                {
                    continue;
                }

                for (const FClimbGraphPoint& DenseB : GeneratedPoints)
                {
                    if (DenseB.Route != PointB.Route ||
                        DenseB.SourceSpline != PointB.SourceSpline)
                    {
                        continue;
                    }

                    if (FVector::DistSquared(
                            DenseB.SurfacePoint, PointB.SurfacePoint) <=
                        FMath::Square(Radius) &&
                        FVector::DotProduct(
                            DenseA.SurfaceNormal,
                            DenseB.SurfaceNormal) >= 0.5f)
                    {
                        bDenseConnectionExists = true;
                        break;
                    }
                }

                if (bDenseConnectionExists)
                {
                    break;
                }
            }

            if (bDenseConnectionExists)
            {
                PointA.Neighbors.AddUnique(B);
                PointB.Neighbors.AddUnique(A);
            }
        }
    }
}

void UClimbingComponent::DrawNavigationDebug() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (int32 Index = 0; Index < NavigationPoints.Num(); ++Index)
    {
        const FClimbNavigationPoint& Point = NavigationPoints[Index];

        const FVector Location =
            Point.SurfacePoint + Point.SurfaceNormal * 12.0f;

        const FColor Color =
            Point.bIsEntryPoint ? FColor::Green : FColor::Orange;

        DrawDebugSphere(
            World, Location, 12.0f, 12, Color,
            false, DebugDuration, 0, 2.0f);

        for (const int32 NeighborIndex : Point.Neighbors)
        {
            if (NeighborIndex <= Index ||
                !NavigationPoints.IsValidIndex(NeighborIndex))
            {
                continue;
            }

            const FClimbNavigationPoint& Neighbor =
                NavigationPoints[NeighborIndex];

            DrawDebugLine(
                World,
                Location,
                Neighbor.SurfacePoint + Neighbor.SurfaceNormal * 12.0f,
                FColor::Purple,
                false,
                DebugDuration,
                0,
                3.0f);
        }
    }
}

FVector UClimbingComponent::GetCharacterLocationForNavigationPoint(
    int32 PointIndex) const
{
    if (!NavigationPoints.IsValidIndex(PointIndex))
    {
        return FVector::ZeroVector;
    }

    const FClimbNavigationPoint& Point = NavigationPoints[PointIndex];

    float CapsuleRadius = 42.0f;

    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (const UCapsuleComponent* Capsule =
                Character->GetCapsuleComponent())
        {
            CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        }
    }

    return Point.SurfacePoint +
        Point.SurfaceNormal * (CapsuleRadius + SurfaceOffset);
}

int32 UClimbingComponent::FindBestNavigationNeighbor(
    int32 FromIndex) const
{
    if (!NavigationPoints.IsValidIndex(FromIndex) ||
        ClimbInput.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    const FClimbNavigationPoint& Current =
        NavigationPoints[FromIndex];

    const FVector Normal = Current.SurfaceNormal.GetSafeNormal();

    FVector ClimbUp = FVector::VectorPlaneProject(
        FVector::UpVector, Normal).GetSafeNormal();

    if (ClimbUp.IsNearlyZero())
    {
        ClimbUp = FVector::VectorPlaneProject(
            FVector::ForwardVector, Normal).GetSafeNormal();
    }

    if (ClimbUp.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    const FVector ClimbRight =
        FVector::CrossProduct(ClimbUp, Normal).GetSafeNormal();

    const FVector DesiredDirection =
        (ClimbUp * ClimbInput.Y +
         ClimbRight * ClimbInput.X).GetSafeNormal();

    if (DesiredDirection.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    float BestScore = 0.1f;
    int32 BestIndex = INDEX_NONE;

    for (const int32 NeighborIndex : Current.Neighbors)
    {
        if (!NavigationPoints.IsValidIndex(NeighborIndex))
        {
            continue;
        }

        const FVector Direction =
            (NavigationPoints[NeighborIndex].SurfacePoint -
             Current.SurfacePoint).GetSafeNormal();

        const float Score =
            FVector::DotProduct(DesiredDirection, Direction);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = NeighborIndex;
        }
    }

    return BestIndex;
}