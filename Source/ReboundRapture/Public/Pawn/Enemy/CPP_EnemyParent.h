// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Engine/EngineTypes.h"
#include "CPP_EnemyParent.generated.h"

class UCapsuleComponent;
class UPaperFlipbookComponent;
class UFloatingPawnMovement;

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

	// --- Tuning (optional) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float MaxSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float Accel = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	float Friction = 8.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


};
