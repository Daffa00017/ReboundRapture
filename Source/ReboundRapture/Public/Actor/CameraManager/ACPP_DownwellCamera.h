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
	bool bLockYToZero = true;

protected:
	virtual FVector ComputeDesiredLocation(float DeltaSeconds) const override
	{
		FVector Desired = GetActorLocation();

		if (FollowTarget)
		{
			const FVector T = FollowTarget->GetActorLocation();

			// Horizontal soft zone
			const float dx = T.X - Desired.X;
			if (FMath::Abs(dx) > SoftZoneHalfWidth)
			{
				Desired.X += (dx - FMath::Sign(dx) * SoftZoneHalfWidth);
			}

			// Lead below target
			Desired.Z = T.Z - DownLead;
		}

		// Auto-scroll downward
		Desired.Z -= AutoScrollSpeed * DeltaSeconds;

		if (bLockYToZero) Desired.Y = 0.f;
		return Desired;
	}
};
