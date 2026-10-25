// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClimbRoute.generated.h"

class USceneComponent;
class USplineComponent;

UCLASS(Blueprintable)
class CLIMBSYSTEM_API AClimbRoute : public AActor
{
	GENERATED_BODY()

public:
	AClimbRoute();
	
protected: 
	virtual void BeginPlay() override;
	
public:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climb Route")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climb Route")
	TObjectPtr<USplineComponent> MainSpline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climb Route|Generation",
		meta=(ClampMin="5.0"))
	float PointSpacing = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climb Route|Generation",
		meta=(ClampMin="10.0"))
	float ProjectionDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climb Route|Generation",
		meta=(ClampMin="1.0"))
	float JunctionRadius = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climb Route|Generation")
	TEnumAsByte<ECollisionChannel> ProjectionTraceChannel = ECC_Visibility;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climb Route|Entries")
	bool bStartIsEntry = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climb Route|Entries")
	bool bEndIsEntry = false;
	
};
