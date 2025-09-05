//Behavior Tree Task Maintain Distance CPP

#include "AI/BTT/BTT_MaintainDistance2D.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/CapsuleComponent.h"
#include "PaperFlipbookComponent.h"
#include "GameFramework/Actor.h"

UBTT_MaintainDistance2D::UBTT_MaintainDistance2D()
{
	NodeName = TEXT("Maintain Distance (2D)");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTT_MaintainDistance2D::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!BB || !AIC) return EBTNodeResult::Failed;

	Pawn = AIC->GetPawn();
	Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Pawn.IsValid() || !Target.IsValid()) return EBTNodeResult::Failed;

	StartTime = Pawn->GetWorld()->GetTimeSeconds();
	bHasGoalX = false;
	GoalX = 0.f;

	return EBTNodeResult::InProgress;
}

void UBTT_MaintainDistance2D::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
	if (!Pawn.IsValid() || !Target.IsValid())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UWorld* W = Pawn->GetWorld();
	APawn* P = Pawn.Get();

	const FVector Me = P->GetActorLocation();
	const FVector To = Target->GetActorLocation();

	float d = DistXZ(Me, To);
	const float dx = To.X - Me.X;

	// --- Outside band: close in / back off ---
	if (d > PreferMax + BandEpsilon)
	{
		bHasGoalX = false;
		Flip(P, dx);
		P->AddMovementInput(FVector(FMath::Sign(dx), 0.f, 0.f), 1.f);
		if (bSnapToGround) Snap(P);
		return;
	}
	if (d < PreferMin - BandEpsilon)
	{
		bHasGoalX = false;
		Flip(P, -dx);
		P->AddMovementInput(FVector(-FMath::Sign(dx), 0.f, 0.f), 1.f);
		if (bSnapToGround) Snap(P);
		return;
	}

	// --- Inside band: either move to a post-shot strafe goal, or succeed (allow fire) ---
	if (!bRandomizeInBand)
	{
		if (auto* M = P->FindComponentByClass<UFloatingPawnMovement>()) M->StopMovementImmediately();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	const bool bRequestNew = (!RequestNewStrafeKey.SelectedKeyName.IsNone())
		? BB->GetValueAsBool(RequestNewStrafeKey.SelectedKeyName)
		: false;

	// Only pick a new strafe goal if Fire requested it (after shot), and we don't already have one
	if (bRequestNew && !bHasGoalX)
	{
		float NewGoalX = 0.f;
		if (ChooseNewStrafeGoal(W, Me, To, NewGoalX))
		{
			GoalX = NewGoalX;
			bHasGoalX = true;

			// consume request so we only randomize once per shot
			BB->SetValueAsBool(RequestNewStrafeKey.SelectedKeyName, false);

			// optional debug spot
			if (!StrafeSpotKey.SelectedKeyName.IsNone())
			{
				FVector Ground;
				if (ProjectGroundAtX(W, NewGoalX, Me.Z, Me.Y, Ground))
				{
					BB->SetValueAsVector(StrafeSpotKey.SelectedKeyName, Ground);
				}
			}
		}
		else
		{
			// couldn't find a nice spot; just let Fire proceed
			if (auto* M = P->FindComponentByClass<UFloatingPawnMovement>()) M->StopMovementImmediately();
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	// If we have a strafe goal, move toward it; else we’re free to shoot this tick
	if (bHasGoalX)
	{
		const float DirX = FMath::Sign(GoalX - Me.X);
		Flip(P, DirX);
		P->AddMovementInput(FVector(DirX, 0.f, 0.f), 1.f);
		if (bSnapToGround) Snap(P);

		if (FMath::Abs(Me.X - GoalX) <= StrafeEpsilon)
		{
			bHasGoalX = false; // clear so we don't keep driving
			if (auto* M = P->FindComponentByClass<UFloatingPawnMovement>()) M->StopMovementImmediately();
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
		return;
	}

	// No strafe requested → already in band → succeed so Fire node can run
	if (auto* M = P->FindComponentByClass<UFloatingPawnMovement>()) M->StopMovementImmediately();
	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTT_MaintainDistance2D::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	if (Pawn.IsValid())
	{
		if (auto* M = Pawn->FindComponentByClass<UFloatingPawnMovement>())
		{
			M->StopMovementImmediately();
		}
	}
	return EBTNodeResult::Aborted;
}

// ================= helpers =================

float UBTT_MaintainDistance2D::DistXZ(const FVector& A, const FVector& B)
{
	FVector D = A - B; D.Y = 0.f; return D.Size();
}

void UBTT_MaintainDistance2D::Snap(APawn* P)
{
	const FVector L = P->GetActorLocation();
	const FVector S = L + FVector(0, 0, 20);
	const FVector E = S + FVector(0, 0, -GroundTrace);

	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(Maintain_Snap), false, P);

	if (P->GetWorld()->LineTraceSingleByChannel(Hit, S, E, GroundChannel, Q))
	{
		float HH = 0.f;
		if (const UCapsuleComponent* C = P->FindComponentByClass<UCapsuleComponent>())
		{
			HH = C->GetScaledCapsuleHalfHeight();
		}
		P->SetActorLocation(FVector(L.X, L.Y, Hit.ImpactPoint.Z + HH), /*bSweep=*/true);
	}
}

void UBTT_MaintainDistance2D::Flip(APawn* P, float DX)
{
	if (!bFlipSpriteX || FMath::Abs(DX) < 1.f) return;

	if (UPaperFlipbookComponent* S = P->FindComponentByClass<UPaperFlipbookComponent>())
	{
		const bool Right = (DX >= 0.f);
		FVector Sc = S->GetRelativeScale3D();
		Sc.X = FMath::Abs(Sc.X) * (Right ? 1.f : -1.f);
		S->SetRelativeScale3D(Sc);
	}
}

bool UBTT_MaintainDistance2D::ChooseNewStrafeGoal(UWorld* W, const FVector& Me, const FVector& Player, float& OutGoalX)
{
	// random side; distance within band + jitter from player.x
	const bool  Right = FMath::RandBool();
	const float Base = FMath::FRandRange(PreferMin, PreferMax);
	const float J = (StrafeJitter > 0.f) ? FMath::FRandRange(-StrafeJitter, +StrafeJitter) : 0.f;

	float DesiredX = Player.X + (Right ? +1.f : -1.f) * (Base + J);

	// ensure meaningful slide
	if (FMath::Abs(DesiredX - Me.X) < MinDeltaXFromMe)
		DesiredX += (Right ? +1.f : -1.f) * MinDeltaXFromMe;

	// project to ground at that X (same Y as me); optionally require LOS
	FVector Ground;
	if (!ProjectGroundAtX(W, DesiredX, Me.Z, Me.Y, Ground)) return false;

	if (bRequireLOSForStrafe)
	{
		AActor* TargetPtr = Target.Get();
		if (!TargetPtr) return false;

		const FVector Eye = Ground + FVector(0, 0, EyeHeight);
		if (!HasLOSFrom(W, Eye, TargetPtr)) return false;
	}

	OutGoalX = Ground.X;
	return true;
}

bool UBTT_MaintainDistance2D::ProjectGroundAtX(UWorld* W, float X, float StartZ, float Y, FVector& OutGround) const
{
	const float Up = GroundTrace * 0.5f;
	const FVector Start(X, Y, StartZ + Up);
	const FVector End(X, Y, StartZ - GroundTrace);

	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(Strafe_Ground), false, Pawn.Get());

	if (W->LineTraceSingleByChannel(Hit, Start, End, GroundChannel, Q))
	{
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool UBTT_MaintainDistance2D::HasLOSFrom(UWorld* W, const FVector& From, const AActor* To) const
{
	if (!W || !To) return false;

	const FVector P = To->GetActorLocation();
	FVector D = P - From; D.Y = 0.f; // XZ plane
	const float Dist = D.Size();
	if (Dist <= KINDA_SMALL_NUMBER) return true;

	const FVector End = From + D.GetSafeNormal() * (Dist + 1.f);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(Strafe_LOS), false, Pawn.Get());

	const bool bBlocked = W->LineTraceSingleByChannel(Hit, From, End, GroundChannel, Q);
	return (!bBlocked || Hit.GetActor() == To);
}
