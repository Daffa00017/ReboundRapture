// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperZDCharacter.h"
#include "CPP_PaperZDParentCharacter.generated.h"

/**
 * 
 */
UCLASS()
class REBOUNDRAPTURE_API ACPP_PaperZDParentCharacter : public APaperZDCharacter
{
	GENERATED_BODY()


public:
    ACPP_PaperZDParentCharacter();

    // Function to update controller rotation based on movement
    UFUNCTION(BlueprintCallable, Category = "Movement") // Added UFUNCTION
        void UpdateControllerRotation();

    UFUNCTION(BlueprintCallable, Category = "Movement") // Added UFUNCTION
        void SetFacingDirection(bool bNewFacingLeft);

    UFUNCTION(BlueprintCallable, Category = "Combat|Effects")
    void StartHitStop(float Duration);

    UFUNCTION(BlueprintCallable, Category = "Combat|Effects")
    void ClearHitStop();

    UCharacterMovementComponent* MyMovementComp;
    AController* MyControllerComp;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;


protected:



    // Variable to track if we're looking back
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    bool bLookingBack;

    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    bool UseControlRotation;

    // HitStop
    FTimerHandle HitStopTimerHandle;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float FacingDeadZone = 10.0f;

    uint8 ebLookingBack : 1;

    // Sprite Shake
    FTimerHandle ShakeTimerHandle;
    FVector OriginalSpriteLocation;
    float CurrentShakeIntensity;
    float ShakeTimeRemaining;


};