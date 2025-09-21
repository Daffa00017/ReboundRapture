// CPP_CameraManager.cpp  (BASE)
#include "Actor/CPP_CameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ACPP_CameraManager::ACPP_CameraManager()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	SetRootComponent(RootComp);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bDoCollisionTest = false;          // base: no camera collision
	SpringArm->SetUsingAbsoluteRotation(true);    // stable orientation

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ACPP_CameraManager::BeginPlay()
{
	Super::BeginPlay();

	// Apply initial knobs
	SpringArm->TargetArmLength = ArmLength;
	SpringArm->AddLocalOffset(ArmLocalOffset);
	SpringArm->SetRelativeRotation(ArmRotation);

	// If no explicit target, default to player 0 pawn
	if (!FollowTarget)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			FollowTarget = PC ? PC->GetPawn() : nullptr;
		}
	}

	// Projection
	Camera->ProjectionMode = bUseOrthographic ? ECameraProjectionMode::Orthographic
		: ECameraProjectionMode::Perspective;
	if (bUseOrthographic)
	{
		Camera->OrthoWidth = OrthoWidth;
	}

	// If you liked your SpringArm lag feeling, keep it here:
	// SpringArm->bEnableCameraLag = true;
	// SpringArm->CameraLagSpeed   = /* CamLagSpeed equivalent */;
}

void ACPP_CameraManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Desired = ComputeDesiredLocation(DeltaSeconds);
	const FVector NewLoc = FMath::VInterpTo(GetActorLocation(), Desired, DeltaSeconds, FollowLerpSpeed);
	SetActorLocation(NewLoc);

	// Keep knobs hot if you tweak at runtime
	SpringArm->TargetArmLength = ArmLength;
	SpringArm->SetRelativeRotation(ArmRotation);
	Camera->ProjectionMode = bUseOrthographic ? ECameraProjectionMode::Orthographic
		: ECameraProjectionMode::Perspective;
	if (bUseOrthographic) { Camera->OrthoWidth = OrthoWidth; }
}

void ACPP_CameraManager::SetFollowTarget(AActor* NewTarget)
{
	FollowTarget = NewTarget;
}

void ACPP_CameraManager::AdoptAsViewTarget(float BlendTime)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		PC->SetViewTargetWithBlend(this, BlendTime);
	}
}
