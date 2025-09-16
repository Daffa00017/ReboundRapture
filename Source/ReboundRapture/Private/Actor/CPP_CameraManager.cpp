// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CPP_CameraManager.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SceneComponent.h"

// Sets default values
ACPP_CameraManager::ACPP_CameraManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    // Initialize components
    RootComp = CreateDefaultSubobject<USceneComponent>("RootComp");
    RootComponent = RootComp; // Set as root

    Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
    Sphere->SetupAttachment(RootComp);
    Sphere->SetSphereRadius(32.0f); // Adjust size as needed

    SpringArm = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
    SpringArm->SetupAttachment(Sphere);




    Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

    // Default variables
    IsFollow = true;
    CanLookAround = false;

}

// Called when the game starts or when spawned
void ACPP_CameraManager::BeginPlay()
{
	Super::BeginPlay();

    SpringArm->AddLocalOffset(FVector(Camoffset));
    SpringArm->TargetArmLength = CamArmLength; // Default camera distance
    SpringArm->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;  // Smooth follow
    SpringArm->CameraLagSpeed = CamLagSpeed;    // Adjust lag speed

}

bool ACPP_CameraManager::LineTraceForObstruction(FVector Start, FVector End)
{
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

    FHitResult HitResult;
    bool bHit = UKismetSystemLibrary::LineTraceSingle(
        GetWorld(),
        Start,
        End,
        ETraceTypeQuery::TraceTypeQuery1,  // Adjust trace channel as needed
        false,  // bTraceComplex
        ActorsToIgnore,
        EDrawDebugTrace::None,  // Disable debug for final build
        HitResult,
        true  // bIgnoreSelf
    );
    return bHit;
}

void ACPP_CameraManager::FollowPlayer(float DeltaTime)
{
    ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (!PlayerCharacter) return;

    // Get player location
    FVector PlayerLocation = PlayerCharacter->GetActorLocation();

    // Calculate camera target position (adjust Z offset as needed)
    FVector CameraTarget = PlayerLocation + FVector(0, 0, 8.7f);  // Your Z-offset

    // Check if there's an obstruction between camera and player
    FVector TraceStart = PlayerLocation;
    FVector TraceEnd = TraceStart + (PlayerCharacter->GetActorUpVector() * -1000000.0f); // Your trace logic

    bool bObstructed = LineTraceForObstruction(TraceStart, TraceEnd);

    if (bObstructed && IsFollow)
    {
        UpdateCameraPosition(CameraTarget, DeltaTime, 5.0f); // Your interpolation speed
    }
}

void ACPP_CameraManager::UpdateCameraPosition(FVector TargetLocation, float DeltaTime, float InterpSpeed)
{
    FVector CurrentLocation = GetActorLocation();
    FVector NewLocation = UKismetMathLibrary::VInterpTo(
        CurrentLocation,
        TargetLocation,
        DeltaTime,
        InterpSpeed
    );

    // Update camera position (with sweep if needed)
    SetActorLocation(NewLocation, true); // bSweep = true
}


// Called every frame
void ACPP_CameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (IsFollow) {
        FollowPlayer(DeltaTime);
    }
}

