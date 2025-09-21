// CPP_CameraManager.h  (now the BASE)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "CPP_CameraManager.generated.h"

UCLASS(Abstract) // <- base; you place/spawn the derived classes instead
class REBOUNDRAPTURE_API ACPP_CameraManager : public AActor
{
	GENERATED_BODY()
public:
	ACPP_CameraManager();

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* Camera;

	// --- Common knobs (renamed to be generic) ---
	/** Replaces CamArmLength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | Rig | Arm")
	float ArmLength = 1200.f;

	/** Replaces Camoffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | Rig | Arm")
	FVector ArmLocalOffset = FVector::ZeroVector;

	/** Base smoothing for rig movement (replaces “interp speed” path) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | Rig | Follow")
	float FollowLerpSpeed = 6.f;

	/** Use orthographic for 2D modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | Rig | Projection")
	bool bUseOrthographic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUseOrthographic"), Category = "Variable | Rig | Projection")
	float OrthoWidth = 2048.f;

	/** Side-scroller look direction; you can tweak per child */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | Rig | Arm")
	FRotator ArmRotation = FRotator(0.f, -90.f, 0.f);

	/** Who we follow (usually the player pawn) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variable | Rig | Follow")
	AActor* FollowTarget = nullptr;

	// Helpers
	UFUNCTION(BlueprintCallable, Category = "Rig")
	void SetFollowTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Rig")
	void AdoptAsViewTarget(float BlendTime = 0.25f);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Children define their desired location. NO side effects. */
	virtual FVector ComputeDesiredLocation(float DeltaSeconds) const PURE_VIRTUAL(ACPP_CameraManager::ComputeDesiredLocation, return GetActorLocation(););

private:
	// Removed: Sphere, LineTraceForObstruction, FollowPlayer, UpdateCameraPosition, CanLookAround, IsFollow
	// Keep SpringArm camera lag if you like it (configure in BeginPlay or per child)
};
