// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_PlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"

ACPP_PlayerCharacter::ACPP_PlayerCharacter()
{
    // Set this character to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;

    // Create ShieldComponent instance

    ViewportSize = FVector2D::ZeroVector;
}

void ACPP_PlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    CachedPlayerController = GetWorld()->GetFirstPlayerController();
    if (CachedPlayerController)
        {
            // Temporary int32 variables for GetViewportSize
            int32 ViewportWidth = 0, ViewportHeight = 0;
            CachedPlayerController->GetViewportSize(ViewportWidth, ViewportHeight);

            // Convert to float and store in FVector2D
            ViewportSize.X = static_cast<float>(ViewportWidth);
            ViewportSize.Y = static_cast<float>(ViewportHeight);

            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(CachedPlayerController->GetLocalPlayer()))
            {
                if (DefaultMappingContext) // Assign in editor or load via ConstructorHelpers
                {
                    Subsystem->AddMappingContext(DefaultMappingContext, 0);
                }
            }
        }
        ThresholdRatio = CursorRotationThreshold / 1920.f;
}

void ACPP_PlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const bool bShouldUpdate = (CurrentTime - LastRotationUpdateTime) >= RotationUpdateInterval;

        UpdateRotationBasedOnCursor();

    if (IsPlayerControlled() && UseControlRotation)
    {
        LastRotationUpdateTime = CurrentTime; // Update timestamp
        UpdateControllerRotation();
    }
}

void ACPP_PlayerCharacter::UpdateRotationBasedOnCursor()
{
    if (!CachedPlayerController) return;

    // Get mouse and character positions in screen space
    FVector2D MousePos;
    FVector2D CharScreenPos;

    if (!CachedPlayerController->GetMousePosition(MousePos.X, MousePos.Y) ||
        !CachedPlayerController->ProjectWorldLocationToScreen(
            GetActorLocation(),  // Using simple actor location instead of capsule
            CharScreenPos,
            true))
    {
        return;
    }

    // Simple X comparison - no viewport size needed
    const bool bNewDirection = (MousePos.X < CharScreenPos.X);

    if (bLookingBack != bNewDirection)
    {
        SetFacingDirection(bNewDirection);
    }
}


void ACPP_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    CachedEIC = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

    // bind both actions on Started & Completed so we track held state
    if (MoveLeftAction)
    {
        CachedEIC->BindAction(MoveLeftAction, ETriggerEvent::Triggered, this, &ACPP_PlayerCharacter::OnMoveLeft);
        CachedEIC->BindAction(MoveLeftAction, ETriggerEvent::Completed, this, &ACPP_PlayerCharacter::OnMoveLeft);
    }
    if (MoveRightAction)
    {
        CachedEIC->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ACPP_PlayerCharacter::OnMoveRight);
        CachedEIC->BindAction(MoveRightAction, ETriggerEvent::Completed, this, &ACPP_PlayerCharacter::OnMoveRight);
    }
}

bool ACPP_PlayerCharacter::GetCharacterScreenPosition(FVector2D& OutPos) const
{
    return GetWorld()->GetFirstPlayerController()->ProjectWorldLocationToScreen(
        GetActorLocation() + FVector(0, 0, 50), // Small height offset
        OutPos
    );
}

void ACPP_PlayerCharacter::MoveHorizontal(const FInputActionValue& Value)
{
    // Get Axis value (-1 to 1)
    const float AxisValue = Value.Get<float>();

    // Add movement input along X axis for side-scroller
    AddMovementInput(FVector(1.f, 0.f, 0.f), AxisValue);
}

void ACPP_PlayerCharacter::OnMoveLeft(const FInputActionValue& Value)
{
    // Digital → bool
    bLeftHeld = Value.Get<bool>();
    RecomputeAxisAndSpeed();
}

void ACPP_PlayerCharacter::OnMoveRight(const FInputActionValue& Value)
{
    // Digital → bool
    bRightHeld = Value.Get<bool>();
    RecomputeAxisAndSpeed();
}

void ACPP_PlayerCharacter::RecomputeAxisAndSpeed()
{
    // merge to one axis (Unity-style)
    const int32 L = bLeftHeld ? 1 : 0;
    const int32 R = bRightHeld ? 1 : 0;
    MoveAxis = float(R - L); // left=-1, right=+1, both=0

    // move character horizontally (Paper2D side scroller → X axis)
    AddMovementInput(FVector(1.f, 0.f, 0.f), MoveAxis);

    // update speed for animation
    AnimSpeedX = FMath::Abs(GetVelocity().X);
}
