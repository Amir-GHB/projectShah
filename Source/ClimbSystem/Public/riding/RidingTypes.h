// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RidingTypes.generated.h"

UENUM(BlueprintType)
enum class ERidingState : uint8
{
	Unmounted UMETA(DisplayName = "Unmounted"),
	Mounting UMETA(DisplayName = "Mounting"),
	Mounted UMETA(DisplayName = "Mounted"),
	Dismounting UMETA(DisplayName = "Dismounting"),
	Falling UMETA(DisplayName = "Falling")
};

UENUM(BlueprintType)
enum class EHorseMountDirection : uint8
{
	None UMETA(DisplayName = "None"),
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
	Center UMETA(DisplayName = "Center / Rear")
};
