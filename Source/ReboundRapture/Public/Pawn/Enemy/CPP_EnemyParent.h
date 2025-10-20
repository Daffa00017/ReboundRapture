// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Engine/EngineTypes.h"
#include "Enum/AIMovementState.h"
#include "Components/HealthComponent.h"
#include "CPP_EnemyParent.generated.h"

class UCapsuleComponent;
class UPaperZDAnimationComponent;
class UPaperZDAnimSequence;
class UPaperZDAnimInstance;
class UPaperFlipbookComponent;
class UFloatingPawnMovement;
class HealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAIMoveStateChanged, EAIMovementState, Old, EAIMovementState, New);

UCLASS()
class REBOUNDRAPTURE_API ACPP_EnemyParent : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ACPP_EnemyParent();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPaperFlipbookComponent* Sprite;

	/** Lightweight movement good for 2D pawns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	UFloatingPawnMovement* MoveComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComp;

	/** Current movement state for PaperZD, UI, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|State")
	EAIMovementState AIMoveState = EAIMovementState::Idle;

	/** Lightweight change API (no tick). Safe to call from tasks/services. */
	UFUNCTION(BlueprintCallable, Category = "AI|State")
	void SetAIMoveState(EAIMovementState NewState);

	UPROPERTY(BlueprintAssignable, Category = "AI|State")
	FOnAIMoveStateChanged OnAIMoveStateChanged;

	// --- Tuning (optional) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float MaxSpeed = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float RunSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float Accel = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float Friction = 8.f;

	// Drive the arm flipbook via PaperZD
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Anim")
	TObjectPtr<UPaperZDAnimationComponent> BodyAnim;

	// Optional: default anim instance class just for the arm (if you use state machines)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Anim")
	TSubclassOf<UPaperZDAnimInstance> BodyAnimInstanceClass;

	//Pool Enemy

	bool bActive = false;

	UFUNCTION(BlueprintCallable) bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintCallable)
	void ActivateFromPool(const FVector& WorldPos)
	{
		SetActorLocation(WorldPos);
		SetActorHiddenInGame(false);
		SetActorTickEnabled(true);
		SetActorEnableCollision(true);
		bActive = true;
		OnPooledActivated();
		// reset movement / state here
	}

	UFUNCTION(BlueprintCallable)
	void DeactivateToPool()
	{
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		SetActorEnableCollision(false);
		bActive = false;
		OnPooledDeactivated();
		// clear targets, velocities, etc.
	}

	// So Blueprint can just call this on “death” (no interface needed)
	UFUNCTION(BlueprintCallable) void RequestDeactivate() 
	{ 
		DeactivateToPool(); 
	}

	// Optional: let BP react to pool events
	UFUNCTION(BlueprintImplementableEvent) void OnPooledActivated();
	UFUNCTION(BlueprintImplementableEvent) void OnPooledDeactivated();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


};
