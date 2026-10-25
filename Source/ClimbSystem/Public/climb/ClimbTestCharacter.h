// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ClimbTestCharacter.generated.h"

class UCameraComponent;
class UClimbRuntimeComponent;
class USpringArmComponent;
class UInputComponent;

/**
 * Development character for validating the route-driven climbing system.
 *
 * Default test controls:
 *   WASD: move on the ground; move along the route while climbing
 *   E: attach to / detach from a nearby authored route
 *   Mouse: look around (uses the project's "Turn" and "LookUp" axis mappings)
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

    UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void ToggleClimbing();
    void TurnCamera(float Value);
    void LookUpCamera(float Value);
    void MoveUpPressed();
    void MoveUpReleased();
    void MoveDownPressed();
    void MoveDownReleased();
    void MoveLeftPressed();
    void MoveLeftReleased();
    void MoveRightPressed();
    void MoveRightReleased();
    void UpdateClimbInput();
    void UpdateGroundMovement();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climbing|Test",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UClimbRuntimeComponent> ClimbRuntimeComponent;

    bool bMoveUp = false;
    bool bMoveDown = false;
    bool bMoveLeft = false;
    bool bMoveRight = false;
};