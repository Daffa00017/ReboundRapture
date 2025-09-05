// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/Enemy/CPP_EnemyParent.h"

#include "Components/CapsuleComponent.h"
#include "PaperFlipbookComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"

// Sets default values
ACPP_EnemyParent::ACPP_EnemyParent()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Root collision
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(18.f, 44.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetCanEverAffectNavigation(false);

	// Visual (Paper2D/PaperZD)
	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(RootComponent);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Movement (lightweight)
	MoveComp = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MoveComp"));
	MoveComp->SetPlaneConstraintEnabled(true);
	// Side-scroller on XZ plane: lock Y
	MoveComp->SetPlaneConstraintNormal(FVector(0.f, 1.f, 0.f));

	// Allow AI possession
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	// We'll set AIControllerClass from C++ or the Blueprint (see controller below)
}

// Called when the game starts or when spawned
void ACPP_EnemyParent::BeginPlay()
{
	Super::BeginPlay();
	
	if (MoveComp)
	{
		MoveComp->MaxSpeed = MaxSpeed;
		MoveComp->Acceleration = Accel;
		MoveComp->Deceleration = FMath::Max(1000.f, Friction * 100.f);
	}
}

// Called every frame
void ACPP_EnemyParent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ACPP_EnemyParent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

