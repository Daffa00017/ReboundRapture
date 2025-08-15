// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_PaperZDParentCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"


ACPP_PaperZDParentCharacter::ACPP_PaperZDParentCharacter()
{
    PrimaryActorTick.bCanEverTick = true; // Enable Tick
    bLookingBack = false;
    UseControlRotation = false;

    bUseControllerRotationYaw = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 500.f); // Fast Z rotation
}


void ACPP_PaperZDParentCharacter::BeginPlay()
{
    Super::BeginPlay();

    MyMovementComp = GetCharacterMovement();
    MyControllerComp = GetController();
}

void ACPP_PaperZDParentCharacter::Tick(float DeltaTime)
{

    
    Super::Tick(DeltaTime);
    if (UseControlRotation) {

        if (CustomTimeDilation <= 0.01f || !MyMovementComp)
            return;

        // Check X velocity directly (faster than IsNearlyZero())
        const float XVelocity = MyMovementComp->Velocity.X;
        if (FMath::Abs(XVelocity) > FacingDeadZone) // Deadzone
            {
                SetFacingDirection(XVelocity < 0); // Directly set direction
            }
    }
}


void ACPP_PaperZDParentCharacter::UpdateControllerRotation()
{
    if (CustomTimeDilation > 0.01f)
    {
        const float XVelocity = MyMovementComp->Velocity.X;
        if (FMath::Abs(XVelocity) > FacingDeadZone)
            SetFacingDirection(XVelocity < 0);
    }
    // When not moving, do nothing - keeps last facing direction
}

void ACPP_PaperZDParentCharacter::SetFacingDirection(bool bNewFacingLeft)
{
    if (bLookingBack == bNewFacingLeft) return; // Early out if no change

    bLookingBack = bNewFacingLeft;

    if (MyControllerComp) // Use cached controller
    {
        const FRotator NewRotation(0.f, bLookingBack ? 180.f : 0.f, 0.f);
        MyControllerComp->SetControlRotation(NewRotation);
        SetActorRotation(NewRotation);
    }
}

void ACPP_PaperZDParentCharacter::StartHitStop(float Duration)
{
    CustomTimeDilation = 0.01f; // Freeze this actor only
    GetWorld()->GetTimerManager().SetTimer(
        HitStopTimerHandle,
        this,
        &ACPP_PaperZDParentCharacter::ClearHitStop,
        Duration,
        false
    );
}

void ACPP_PaperZDParentCharacter::ClearHitStop()
{
    CustomTimeDilation = 1.0f; // Restore
}
