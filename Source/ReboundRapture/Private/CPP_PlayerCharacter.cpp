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
        CachedEIC->BindAction(MoveLeftAction, ETriggerEvent::Started, this, &ACPP_PlayerCharacter::OnMoveLeftStarted);
        CachedEIC->BindAction(MoveLeftAction, ETriggerEvent::Triggered, this, &ACPP_PlayerCharacter::OnMoveLeftTriggered);
        CachedEIC->BindAction(MoveLeftAction, ETriggerEvent::Completed, this, &ACPP_PlayerCharacter::OnMoveLeftCompleted);
    }
    if (MoveRightAction)
    {
        CachedEIC->BindAction(MoveRightAction, ETriggerEvent::Started, this, &ACPP_PlayerCharacter::OnMoveRightStarted);
        CachedEIC->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ACPP_PlayerCharacter::OnMoveRightTriggered);
        CachedEIC->BindAction(MoveRightAction, ETriggerEvent::Completed, this, &ACPP_PlayerCharacter::OnMoveRightCompleted);
    }

    // DASH
    if (DashAction)
    {
        CachedEIC->BindAction(DashAction, ETriggerEvent::Started, this, &ThisClass::OnDashStarted);
        CachedEIC->BindAction(DashAction, ETriggerEvent::Triggered, this, &ThisClass::OnDashTriggered);
        CachedEIC->BindAction(DashAction, ETriggerEvent::Completed, this, &ThisClass::OnDashCompleted);
    }

    // JUMP
    if (JumpAction)
    {
        CachedEIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::OnJumpStarted);
        CachedEIC->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ThisClass::OnJumpTriggered);
        CachedEIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::OnJumpCompleted);
    }

    // SHOOT
    if (ShootAction)
    {
        CachedEIC->BindAction(ShootAction, ETriggerEvent::Started, this, &ThisClass::OnShootStarted);
        CachedEIC->BindAction(ShootAction, ETriggerEvent::Triggered, this, &ThisClass::OnShootTriggered);
        CachedEIC->BindAction(ShootAction, ETriggerEvent::Completed, this, &ThisClass::OnShootCompleted);
    }

    // SLIDE/ROLL
    if (SlideRollAction)
    {
        CachedEIC->BindAction(SlideRollAction, ETriggerEvent::Started, this, &ThisClass::OnSlideRollStarted);
        CachedEIC->BindAction(SlideRollAction, ETriggerEvent::Triggered, this, &ThisClass::OnSlideRollTriggered);
        CachedEIC->BindAction(SlideRollAction, ETriggerEvent::Completed, this, &ThisClass::OnSlideRollCompleted);
    }

    // AIM (hold to aim)
    if (AimAction)
    {
        CachedEIC->BindAction(AimAction, ETriggerEvent::Started, this, &ThisClass::OnAimStarted);
        CachedEIC->BindAction(AimAction, ETriggerEvent::Triggered, this, &ThisClass::OnAimTriggered);
        CachedEIC->BindAction(AimAction, ETriggerEvent::Completed, this, &ThisClass::OnAimCompleted);
    }
}

void ACPP_PlayerCharacter::ChangeMovementState(EMovementState NewState)
{
    if (NewState == CurrentMovementState) return;
    const EMovementState Old = CurrentMovementState;
    CurrentMovementState = NewState;
    OnMovementStateChanged.Broadcast(Old, CurrentMovementState);
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

// === LEFT ===
void ACPP_PlayerCharacter::OnMoveLeftStarted(const FInputActionValue& Value)
{
    bLeftHeld = true;

    const int Combined = GetCombinedAxis();
    const bool bBothHeld = (bLeftHeld && bRightHeld);

    // any directed input cancels idle and commits Walk
    if (Combined != 0)
    {
        GetWorldTimerManager().ClearTimer(IdleConfirmTimer);
        if (CurrentMovementState != EMovementState::Walk)
            ChangeMovementState(EMovementState::Walk);
    }

    // edge: just entered both-held? schedule idle once
    if (bBothHeld && !bWasBothHeld)
    {
        ScheduleIdleConfirm();
        bWasBothHeld = true;
    }
}

void ACPP_PlayerCharacter::OnMoveLeftTriggered(const FInputActionValue& Value)
{
    bLeftHeld = true;
    RecomputeAxisAndSpeed();

    const int Combined = GetCombinedAxis();
    const bool bBothHeld = (bLeftHeld && bRightHeld);

    if (Combined != 0)
    {
        // directed → keep/cancel idle
        GetWorldTimerManager().ClearTimer(IdleConfirmTimer);
        if (CurrentMovementState != EMovementState::Walk)
            ChangeMovementState(EMovementState::Walk);
    }
    else
    {
        // undirected (both-held) — only schedule on edge
        if (bBothHeld && !bWasBothHeld)
        {
            ScheduleIdleConfirm();
            bWasBothHeld = true;
        }
    }
}

void ACPP_PlayerCharacter::OnMoveLeftCompleted(const FInputActionValue& Value)
{
    bLeftHeld = false;
    RecomputeAxisAndSpeed();

    const int Combined = GetCombinedAxis();
    const bool bBothHeld = (bLeftHeld && bRightHeld);

    if (!bRightHeld)
    {
        // both released → schedule idle
        ScheduleIdleConfirm();
    }
    else
    {
        // we left both-held? clear the pending idle
        if (!bBothHeld && bWasBothHeld)
        {
            GetWorldTimerManager().ClearTimer(IdleConfirmTimer);
            bWasBothHeld = false;
        }

        // if now directed, ensure Walk
        if (Combined != 0 && CurrentMovementState != EMovementState::Walk)
            ChangeMovementState(EMovementState::Walk);
    }
}

// === RIGHT (mirror) ===
void ACPP_PlayerCharacter::OnMoveRightStarted(const FInputActionValue& Value)
{
    bRightHeld = true;

    const int Combined = GetCombinedAxis();
    const bool bBothHeld = (bLeftHeld && bRightHeld);

    if (Combined != 0)
    {
        GetWorldTimerManager().ClearTimer(IdleConfirmTimer);
        if (CurrentMovementState != EMovementState::Walk)
            ChangeMovementState(EMovementState::Walk);
    }
    if (bBothHeld && !bWasBothHeld)
    {
        ScheduleIdleConfirm();
        bWasBothHeld = true;
    }
}

void ACPP_PlayerCharacter::OnMoveRightTriggered(const FInputActionValue& Value)
{
    bRightHeld = true;
    RecomputeAxisAndSpeed();

    const int Combined = GetCombinedAxis();
    const bool bBothHeld = (bLeftHeld && bRightHeld);

    if (Combined != 0)
    {
        GetWorldTimerManager().ClearTimer(IdleConfirmTimer);
        if (CurrentMovementState != EMovementState::Walk)
            ChangeMovementState(EMovementState::Walk);
    }
    else
    {
        if (bBothHeld && !bWasBothHeld)
        {
            ScheduleIdleConfirm();
            bWasBothHeld = true;
        }
    }
}

void ACPP_PlayerCharacter::OnMoveRightCompleted(const FInputActionValue& Value)
{
    bRightHeld = false;
    RecomputeAxisAndSpeed();

    const int Combined = GetCombinedAxis();
    const bool bBothHeld = (bLeftHeld && bRightHeld);

    if (!bLeftHeld)
    {
        ScheduleIdleConfirm(); // both released
    }
    else
    {
        if (!bBothHeld && bWasBothHeld)
        {
            GetWorldTimerManager().ClearTimer(IdleConfirmTimer);
            bWasBothHeld = false;
        }

        if (Combined != 0 && CurrentMovementState != EMovementState::Walk)
            ChangeMovementState(EMovementState::Walk);
    }
}
void ACPP_PlayerCharacter::RecomputeAxisAndSpeed()
{
    const int Combined = GetCombinedAxis();
    MoveAxis = float(Combined);              // -1, 0, +1

    if (Combined != 0)
        AddMovementInput(FVector(1, 0, 0), MoveAxis);

    const float NewSpeedX = FMath::Abs(GetVelocity().X);
    AnimSpeedX = NewSpeedX;

    if (!FMath::IsNearlyEqual(MoveAxis, LastAxisSent, AxisEpsilon))
    {
        LastAxisSent = MoveAxis;
        OnMoveAxisUpdatedDelegate.Broadcast(MoveAxis, AnimSpeedX);
    }
}

void ACPP_PlayerCharacter::UpdateMovementStateFromAxis()
{
    const bool bAnyHeld = bLeftHeld || bRightHeld;            // ← key idea
    const bool bMovingBySpeed = AnimSpeedX > WalkSpeedThreshold;

    EMovementState Desired = CurrentMovementState;

    if (bAnyHeld || bMovingBySpeed)
    {
        // If any input is held (even if MoveAxis == 0 due to both pressed),
        // or we’re still moving by speed, stay in Walk.
        Desired = EMovementState::Walk;
        TimeSinceInputReleased = 0.f; // reset just in case
    }
    else
    {
        // Both inputs released — require a short confirm delay before Idle
        if (TimeSinceInputReleased >= IdleConfirmDelay)
        {
            Desired = EMovementState::Idle;
        }
        // else: keep previous state (usually Walk) during the grace window
    }

    if (Desired != CurrentMovementState)
    {
        const EMovementState Old = CurrentMovementState;
        CurrentMovementState = Desired;
        OnMovementStateChanged.Broadcast(Old, CurrentMovementState);
    }
}

void ACPP_PlayerCharacter::ScheduleIdleConfirm()
{
    GetWorldTimerManager().ClearTimer(IdleConfirmTimer);
    GetWorldTimerManager().SetTimer(
        IdleConfirmTimer, this, &ACPP_PlayerCharacter::ConfirmIdle,
        IdleConfirmDelay, false
    );
}

void ACPP_PlayerCharacter::ConfirmIdle()
{
    if (GetCombinedAxis() == 0) // both-held OR none-held
    {
        ChangeMovementState(EMovementState::Idle);
    }
}

void ACPP_PlayerCharacter::CommitWalkIfStillDirected()
{
    const int CombinedAxis = GetCombinedAxis();
    if (CombinedAxis != 0 && CurrentMovementState != EMovementState::Walk)
    {
        ChangeMovementState(EMovementState::Walk);
    }
}

// === DASH ===
void ACPP_PlayerCharacter::OnDashStarted(const FInputActionValue& Value)
{
    bDashInput = true;
    OnDashStartedEvent.Broadcast(GetScalar01(Value));
    // TODO: actually start dash (set state, launch, i-frames, etc.)
}

void ACPP_PlayerCharacter::OnDashTriggered(const FInputActionValue& Value)
{
    bDashInput = true;
    // Optional: sustain dash if analog/hold dash
}

void ACPP_PlayerCharacter::OnDashCompleted(const FInputActionValue& /*Value*/)
{
    bDashInput = false;
    OnDashCompletedEvent.Broadcast();
    // TODO: end dash (clear state)
}

// === JUMP ===
void ACPP_PlayerCharacter::OnJumpStarted(const FInputActionValue& Value)
{
    bJumpInput = true;
    OnJumpStartedEvent.Broadcast(GetScalar01(Value));
    // TODO: Call Jump(); or custom jump logic
    Jump();
}

void ACPP_PlayerCharacter::OnJumpTriggered(const FInputActionValue& Value)
{
    bJumpInput = true;
    // Optional: variable jump height (hold duration), coyote time, etc.
}

void ACPP_PlayerCharacter::OnJumpCompleted(const FInputActionValue& /*Value*/)
{
    bJumpInput = false;
    OnJumpCompletedEvent.Broadcast();
    // Optional: StopJumping(); for variable height
    StopJumping();
}

// === SHOOT ===
void ACPP_PlayerCharacter::OnShootStarted(const FInputActionValue& Value)
{
    bShootInput = true;
    OnShootStartedEvent.Broadcast(GetScalar01(Value));
    // TODO: start firing / spawn projectile / play montage
}

void ACPP_PlayerCharacter::OnShootTriggered(const FInputActionValue& Value)
{
    bShootInput = true;
    // Optional: auto-fire if action is set to "Repeat while held"
}

void ACPP_PlayerCharacter::OnShootCompleted(const FInputActionValue& /*Value*/)
{
    bShootInput = false;
    OnShootCompletedEvent.Broadcast();
    // TODO: stop firing, cooldown end, etc.
}

// === SLIDE / ROLL ===
void ACPP_PlayerCharacter::OnSlideRollStarted(const FInputActionValue& Value)
{
    bSlideRollInput = true;
    OnSlideRollStartedEvent.Broadcast(GetScalar01(Value));
    // TODO: start slide/roll
}

void ACPP_PlayerCharacter::OnSlideRollTriggered(const FInputActionValue& Value)
{
    bSlideRollInput = true;
    // Optional: sustain slide if analog
}

void ACPP_PlayerCharacter::OnSlideRollCompleted(const FInputActionValue& /*Value*/)
{
    bSlideRollInput = false;
    OnSlideRollCompletedEvent.Broadcast();
    // TODO: end slide/roll
}

// === AIM (hold to aim) ===
void ACPP_PlayerCharacter::OnAimStarted(const FInputActionValue& Value)
{
    bAimInput = true;
    OnAimStartedEvent.Broadcast(GetScalar01(Value));
    // Optional: bIsAiming = true; adjust FOV/speed/reticle
}

void ACPP_PlayerCharacter::OnAimTriggered(const FInputActionValue& Value)
{
    bAimInput = true;
    // Optional: continuous aim adjustments (e.g., cursor aim move)
}

void ACPP_PlayerCharacter::OnAimCompleted(const FInputActionValue& /*Value*/)
{
    bAimInput = false;
    OnAimCompletedEvent.Broadcast();
    // Optional: bIsAiming = false; restore FOV/speed
}


