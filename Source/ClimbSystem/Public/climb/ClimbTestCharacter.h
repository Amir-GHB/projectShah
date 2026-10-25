// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ClimbTestCharacter.generated.h"

class UClimbRuntimeComponent;
class UInputComponent;

/**
 * Minimal development character for validating the new route-driven climbing system.
 *
 * Default test controls:
 *   E: attach to / detach from a nearby authored route
 *   W/S: move up/down the surface
 *   A/D: move left/right across the surface
 *
 * This is a test harness, not the intended production player character. Create a
 * Blueprint child to assign a skeletal mesh, animation blueprint, camera, and
 * project-specific presentation without coupling those choices to the runtime.
 */
UCLASS(Blueprintable)
class CLIMBSYSTEM_API AClimbTestCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AClimbTestCharacter();

    UFUNCTION(BlueprintPure, Category="Climbing|Test")
    UClimbRuntimeComponent* GetClimbRuntimeComponent() const
    {
        return ClimbRuntimeComponent;
    }

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    void ToggleClimbing();
    void MoveUpPressed();
    void MoveUpReleased();
    void MoveDownPressed();
    void MoveDownReleased();
    void MoveLeftPressed();
    void MoveLeftReleased();
    void MoveRightPressed();
    void MoveRightReleased();
    void UpdateClimbInput();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing|Test",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UClimbRuntimeComponent> ClimbRuntimeComponent;

    bool bMoveUp = false;
    bool bMoveDown = false;
    bool bMoveLeft = false;
    bool bMoveRight = false;
};