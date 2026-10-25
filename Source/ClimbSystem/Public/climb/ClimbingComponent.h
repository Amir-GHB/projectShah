// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClimbingComponent.generated.h"

class AClimbRoute;
class USplineComponent;

USTRUCT(BlueprintType)
struct FClimbGraphPoint
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    FVector SurfacePoint = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    FVector SurfaceNormal = FVector::UpVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    TObjectPtr<AClimbRoute> Route = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    TObjectPtr<USplineComponent> SourceSpline = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    int32 SampleIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    int32 SampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    float DistanceAlongSpline = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    bool bIsEntryPoint = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    TArray<int32> Neighbors;
};

USTRUCT(BlueprintType)
struct FClimbNavigationPoint
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    FVector SurfacePoint = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    FVector SurfaceNormal = FVector::UpVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    TObjectPtr<AClimbRoute> Route = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    TObjectPtr<USplineComponent> SourceSpline = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    int32 SplinePointIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    bool bIsEntryPoint = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing")
    TArray<int32> Neighbors;
};

UCLASS(ClassGroup=(Traversal), meta=(BlueprintSpawnableComponent))
class CLIMBSYSTEM_API UClimbingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UClimbingComponent();
    
    UFUNCTION(BlueprintCallable, Category="Climbing")
    void BuildRouteGraph();
    
    virtual void TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
) override;

    UFUNCTION(BlueprintCallable, Category="Climbing")
    bool StartClimbing();

    UFUNCTION(BlueprintCallable, Category="Climbing")
    void StopClimbing();

    UFUNCTION(BlueprintCallable, Category="Climbing")
    void SetClimbInput(FVector2D Input);
    
protected:
    virtual void BeginPlay() override;
    
private:
    void ConnectGraphPoints();
    void DrawGraphDebug() const;
    
    void BuildNavigationPoints();
    void ConnectNavigationPoints();
    void DrawNavigationDebug() const;

    int32 FindBestNavigationNeighbor(int32 FromIndex) const;

    FVector GetCharacterLocationForNavigationPoint(
        int32 PointIndex) const;
    
public:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient,
        Category="Climbing|Debug")
    TArray<FClimbGraphPoint> GeneratedPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Debug")
    bool bDrawDebugGraph = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Debug",
        meta=(ClampMin="0.0"))
    float DebugDuration = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Movement",
    meta=(ClampMin="1.0"))
    float ClimbSpeed = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Movement",
        meta=(ClampMin="1.0"))
    float MaxAttachDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climbing|Movement",
        meta=(ClampMin="0.0"))
    float SurfaceOffset = 5.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Climbing|Movement")
    bool bIsClimbing = false;
    
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Climbing|Debug")
    TArray<FClimbNavigationPoint> NavigationPoints;
    
private:
    
    
    int32 CurrentPointIndex = INDEX_NONE;
    int32 TargetPointIndex = INDEX_NONE;

    FVector2D ClimbInput = FVector2D::ZeroVector;

    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;

    int32 FindBestNeighbor(int32 FromIndex) const;
    FVector GetCharacterLocationForPoint(int32 PointIndex) const;
    FRotator GetClimbingRotation(const FVector& SurfaceNormal) const;



};
