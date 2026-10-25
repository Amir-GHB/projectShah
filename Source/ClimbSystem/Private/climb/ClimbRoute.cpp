// Fill out your copyright notice in the Description page of Project Settings.


#include "Public/climb/ClimbRoute.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

AClimbRoute::AClimbRoute()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));

	SetRootComponent(SceneRoot);

	MainSpline = CreateDefaultSubobject<USplineComponent>(
		TEXT("MainSpline"));

	MainSpline->SetupAttachment(SceneRoot);
}

// Called when the game starts or when spawned
void AClimbRoute::BeginPlay()
{
	Super::BeginPlay();
	
}

