// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CPP_PaperZDParentCharacter.h"
#include "Components/CapsuleComponent.h"
#include "InputActionValue.h"
#include "Delegates/DelegateCombinations.h"
#include "CPP_PlayerCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;

UENUM(BlueprintType) 
enum class EMovementState : uint8 
{
	Idle UMETA(DisplayName = "Idle"),
	Walk UMETA(DisplayName = "Walk"),
	Aiming UMETA(DisplayName = "Aiming"),
	SlideOrRoll UMETA(DisplayName = "Slide/Roll"),
	WallSlide UMETA(DisplayName = "WallSlide"),
	Dash UMETA(DisplayName = "Dash"),
	Jump UMETA(DisplayName = "jump")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoveAxisUpdated, float, Axis, float, SpeedX);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementStateChanged, EMovementState, OldState, EMovementState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDashStarted, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJumpStarted, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJumpCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShootStarted, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShootCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlideRollStarted, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlideRollCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAimStarted, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAimCompleted);

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

	// --- Action delegate instances ---
	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnDashStarted        OnDashStartedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnDashCompleted      OnDashCompletedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnJumpStarted        OnJumpStartedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnJumpCompleted      OnJumpCompletedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnShootStarted       OnShootStartedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnShootCompleted     OnShootCompletedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnSlideRollStarted   OnSlideRollStartedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnSlideRollCompleted OnSlideRollCompletedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnAimStarted         OnAimStartedEvent;
	UPROPERTY(BlueprintAssignable, Category = "Input|Delegates") FOnAimCompleted       OnAimCompletedEvent;

	

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Controls",
		meta = (ClampMin = "0", ClampMax = "1000", DisplayPriority = "1"))
	float CursorRotationThreshold = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Controls", meta = (DisplayPriority = 1))
	float RotationUpdateInterval = 0.5f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player Controls", meta = (DisplayPriority = 1))
	APlayerController* CachedPlayerController;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	EMovementState CurrentMovementState = EMovementState::Idle;

	UPROPERTY(BlueprintAssignable, Category = "Anim|Delegates")
	FOnMovementStateChanged OnMovementStateChanged;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void ChangeMovementState(EMovementState NewState);

	UPROPERTY(EditAnywhere, Category = "Movement|Tuning")
	float WalkSpeedThreshold = 50.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Tuning")
	float IdleConfirmDelay = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Movement|Tuning")
	float WalkCommitDelay = 0.03f;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float MoveAxis = 0.f;                

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	float AnimSpeedX = 0.f;       

	float LastAxisSent = 9999.f;     // sentinel
	float LastSpeedXSent = -1.f;

	UPROPERTY(EditAnywhere, Category = "Anim|Tuning")
	float AxisEpsilon = 0.001f;      // exact change for digital axis is fine

	UPROPERTY(EditAnywhere, Category = "Anim|Tuning")
	float SpeedEpsilon = 2.0f;       // pixels/uu per sec change before we notify

	UPROPERTY(BlueprintAssignable, Category = "Anim|Delegates")
	FOnMoveAxisUpdated OnMoveAxisUpdatedDelegate;

	//Input Variable
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bLeftHeld = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bRightHeld = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bWasBothHeld = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bDashInput = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bJumpInput = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bShootInput = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bSlideRollInput = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input|State")
	bool bAimInput = false;
	

private:
	float LastRotationUpdateTime = -1.f;
	FORCEINLINE bool GetCharacterScreenPosition(FVector2D& OutPos) const;
	UEnhancedInputComponent* CachedEIC = nullptr;

	//Input Variable

	float TimeSinceInputReleased = 0.f;
	float LastNonZeroAxis = 0.f;
	double LastInputPressedTime = 0.0;
	FTimerHandle IdleConfirmTimer;
	FTimerHandle WalkCommitTimer;


	//Input Asset
	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 1))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 1))
	UInputAction* MoveLeftAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 0))
	UInputAction* MoveRightAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 0))
	UInputAction* DashAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 0))
	UInputAction* JumpAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 0))
	UInputAction* ShootAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 0))
	UInputAction* SlideRollAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (DisplayPriority = 0))
	UInputAction* AimAction = nullptr;

	EMovementState LastMovementState = EMovementState::Idle;


	//Input Function

	//MoveAction (A & D) Input
	void MoveHorizontal(const FInputActionValue& Value);
	void OnMoveLeftTriggered(const FInputActionValue& Value);
	void OnMoveRightTriggered(const FInputActionValue& Value);
	void OnMoveLeftStarted(const FInputActionValue& Value);
	void OnMoveRightStarted(const FInputActionValue& Value);
	void OnMoveLeftCompleted(const FInputActionValue& Value);
	void OnMoveRightCompleted(const FInputActionValue& Value);
		//MoveAction Function To move the player
		void RecomputeAxisAndSpeed();
		void UpdateMovementStateFromAxis();
		void ScheduleIdleConfirm();
		void ConfirmIdle();
		void CommitWalkIfStillDirected();

	// Dash
	void OnDashStarted(const FInputActionValue& Value);
	void OnDashTriggered(const FInputActionValue& Value);
	void OnDashCompleted(const FInputActionValue& Value);

	// Jump
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpTriggered(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);

	// Shoot
	void OnShootStarted(const FInputActionValue& Value);
	void OnShootTriggered(const FInputActionValue& Value);
	void OnShootCompleted(const FInputActionValue& Value);

	// Slide/Roll
	void OnSlideRollStarted(const FInputActionValue& Value);
	void OnSlideRollTriggered(const FInputActionValue& Value);
	void OnSlideRollCompleted(const FInputActionValue& Value);

	// Aim (usually a hold-to-aim)
	void OnAimStarted(const FInputActionValue& Value);
	void OnAimTriggered(const FInputActionValue& Value);
	void OnAimCompleted(const FInputActionValue& Value);

	

	FORCEINLINE int GetCombinedAxis() const
	{
		const int L = bLeftHeld ? 1 : 0;
		const int R = bRightHeld ? 1 : 0;
		return R - L;   // -1 if Left, +1 if Right, 0 if none or both
	}

	static float GetScalar01(const FInputActionValue& V)
	{
		// Bool ? 0/1, 1D Axis ? normalized, others ? magnitude
		if (V.GetValueType() == EInputActionValueType::Boolean)
			return V.Get<bool>() ? 1.f : 0.f;
		if (V.GetValueType() == EInputActionValueType::Axis1D)
			return V.Get<float>();
		if (V.GetValueType() == EInputActionValueType::Axis2D)
			return V.Get<FVector2D>().Size();
		if (V.GetValueType() == EInputActionValueType::Axis3D)
			return V.Get<FVector>().Size();
		return 0.f;
	}

};
