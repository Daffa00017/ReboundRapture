// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/CPP_CameraManager.h"
#include "ACPP_DownwellCamera.generated.h"

/**
 * 
 */
UCLASS()
class REBOUNDRAPTURE_API AACPP_DownwellCamera : public ACPP_CameraManager
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | CameraManager | DownWell")
	float SoftZoneHalfWidth = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | CameraManager | DownWell")
	float DownLead = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | CameraManager | DownWell")
	float AutoScrollSpeed = 320.f; // uu/sec; down is negative Z

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | CameraManager | DownWell")
	float WellCenterX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | CameraManager | DownWell")
	bool bLockYToZero = true;

protected:
	virtual FVector ComputeDesiredLocation(float DeltaSeconds) const override
	{
		FVector Desired = GetActorLocation();

		if (FollowTarget)
		{
			const FVector T = FollowTarget->GetActorLocation();

			// Lock horizontal axis (well center)
			Desired.X = WellCenterX;   // usually 0.0f, or whatever your well center is

			// Lead below target
			Desired.Z = T.Z - DownLead;
		}

		// Auto-scroll downward (keeps moving even if player stalls)
		Desired.Z -= AutoScrollSpeed * DeltaSeconds;

		// Always keep camera centered on well in Y
		if (bLockYToZero) Desired.Y = 0.f;

		return Desired;
	}
};
