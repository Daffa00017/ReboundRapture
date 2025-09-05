// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTT/BTT_MoveToLocation2D_Snap.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/CapsuleComponent.h"
#include "PaperFlipbookComponent.h"

UBTT_MoveToLocation2D_Snap::UBTT_MoveToLocation2D_Snap()
{
	NodeName = TEXT("Move To Location (2D XZ + Snap)");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTT_MoveToLocation2D_Snap::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	auto* BB = OwnerComp.GetBlackboardComponent();
	auto* AIC = OwnerComp.GetAIOwner();
	if (!BB || !AIC) return EBTNodeResult::Failed;

	Pawn = AIC->GetPawn();
	if (!Pawn.IsValid()) return EBTNodeResult::Failed;

	TargetLoc = BB->GetValueAsVector(LocationKey.SelectedKeyName);
	StartTime = Pawn->GetWorld()->GetTimeSeconds();
	return EBTNodeResult::InProgress;
}

void UBTT_MoveToLocation2D_Snap::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float)
{
	if (!Pawn.IsValid()) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }
	APawn* P = Pawn.Get();

	if (Dist2D_XZ(P->GetActorLocation(), TargetLoc) <= AcceptRadius)
	{
		if (auto* M = P->FindComponentByClass<UFloatingPawnMovement>()) M->StopMovementImmediately();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	MoveHorizontalToward(P, TargetLoc);
	if (bSnapToGround) SnapDown(P);

	if (P->GetWorld()->GetTimeSeconds() - StartTime > MaxTime)
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
}

EBTNodeResult::Type UBTT_MoveToLocation2D_Snap::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (Pawn.IsValid())
		if (auto* M = Pawn->FindComponentByClass<UFloatingPawnMovement>()) M->StopMovementImmediately();
	return EBTNodeResult::Aborted;
}

float UBTT_MoveToLocation2D_Snap::Dist2D_XZ(const FVector& A, const FVector& B)
{
	FVector D = A - B; D.Y = 0.f; return D.Size();
}

void UBTT_MoveToLocation2D_Snap::MoveHorizontalToward(APawn* P, const FVector& To)
{
	FVector D = To - P->GetActorLocation(); D.Y = 0.f;
	Flip(P, D.X);
	P->AddMovementInput(FVector(FMath::Sign(D.X), 0, 0), 1.f);
}

void UBTT_MoveToLocation2D_Snap::SnapDown(APawn* P)
{
	const FVector Loc = P->GetActorLocation();
	const FVector Start = Loc + FVector(0, 0, 20);
	const FVector End = Start + FVector(0, 0, -GroundTraceDistance);
	FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(Move2D_Snap), false, P);
	if (P->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel, Q))
	{
		float HalfHeight = 0.f;
		if (auto* Cap = P->FindComponentByClass<UCapsuleComponent>()) HalfHeight = Cap->GetScaledCapsuleHalfHeight();
		FVector NewLoc = Loc; NewLoc.Z = Hit.ImpactPoint.Z + HalfHeight;
		P->SetActorLocation(NewLoc, /*bSweep=*/true);
	}
}

void UBTT_MoveToLocation2D_Snap::Flip(APawn* P, float DX)
{
	if (FMath::Abs(DX) < 1.f) return;
	if (auto* Sprite = P->FindComponentByClass<UPaperFlipbookComponent>())
	{
		const bool Right = DX >= 0.f;
		FVector S = Sprite->GetRelativeScale3D();
		S.X = FMath::Abs(S.X) * (Right ? 1.f : -1.f);
		Sprite->SetRelativeScale3D(S);
	}
}
