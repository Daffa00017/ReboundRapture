// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "CPP_CameraManager.generated.h"

UCLASS()
class REBOUNDRAPTURE_API ACPP_CameraManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACPP_CameraManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", Category = "Camera/Value"))
	float CamArmLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", Category = "Camera/Value"))
	float CamLagSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", Category = "Camera/Value"))
	FVector Camoffset;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;  // Replaces DefaultSceneRoot

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* Sphere;   // Collision sphere

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* SpringArm; // Camera boom

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* Camera;   // Actual camera

	// --- VARIABLES ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool IsFollow;  // Follow player?

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool CanLookAround; // Free look toggle

	// --- FUNCTIONS ---
	bool LineTraceForObstruction(FVector Start, FVector End);
	void FollowPlayer(float DeltaTime);
	void UpdateCameraPosition(FVector TargetLocation, float DeltaTime, float InterpSpeed = 5.0f);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
