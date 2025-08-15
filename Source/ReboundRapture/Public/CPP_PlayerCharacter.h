// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CPP_PaperZDParentCharacter.h"
#include "Components/CapsuleComponent.h"
#include "InputActionValue.h"
#include "CPP_PlayerCharacter.generated.h"


class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;

UENUM(BlueprintType) 
enum class EMovementState : uint8 
{
	Option_A UMETA(DisplayName = "Idle"), 
	Option_B UMETA(DisplayName = "Walk"),
	Option_C UMETA(DisplayName = "Aiming"),
	Option_D UMETA(DisplayName = "Slide/Roll"),
	Option_E UMETA(DisplayName = "WallSlide"),
	Option_F UMETA(DisplayName = "Dash"),
	Option_G UMETA(DisplayName = "jump")
};

/**
 * 
 */
UCLASS()
class REBOUNDRAPTURE_API ACPP_PlayerCharacter : public ACPP_PaperZDParentCharacter
{
	GENERATED_BODY()

public:
	ACPP_PlayerCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	void UpdateRotationBasedOnCursor();

protected:

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, Category = "Player Controls", meta = (DisplayPriority = 1))
	float PlayerDeadZone = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Player Controls", meta = (DisplayPriority = 1))
	float CursorRotationDeadZone = 50.f;

	UPROPERTY()
	FVector2D ViewportSize;
	float ThresholdRatio;


	// Distance from character screen position to trigger rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Controls", meta = (ClampMin = "0", ClampMax = "1000"), meta = (DisplayPriority = 1))
	float CursorRotationThreshold = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Controls", meta = (DisplayPriority = 1))
	float RotationUpdateInterval = 0.5f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player Controls", meta = (DisplayPriority = 1))
	APlayerController* CachedPlayerController;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	EMovementState CurrentMovementState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float MoveAxis = 0.f;                

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	float AnimSpeedX = 0.f;              

private:
	float LastRotationUpdateTime = -1.f;
	FORCEINLINE bool GetCharacterScreenPosition(FVector2D& OutPos) const;
	UEnhancedInputComponent* CachedEIC = nullptr;

	//Input Variable
	bool bLeftHeld = false;
	bool bRightHeld = false;

	//Input Asset
	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 1))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 1))
	UInputAction* MoveLeftAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 0))
	UInputAction* MoveRightAction = nullptr;


	//Input Function
	void MoveHorizontal(const FInputActionValue& Value);
	void OnMoveLeft(const FInputActionValue& Value);
	void OnMoveRight(const FInputActionValue& Value);
	void RecomputeAxisAndSpeed();
};
