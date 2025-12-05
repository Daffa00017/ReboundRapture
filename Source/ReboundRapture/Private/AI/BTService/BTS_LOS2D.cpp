// BTService Line Of Sight 2D CPP


#include "AI/BTService/BTS_LOS2D.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

UBTS_LOS2D::UBTS_LOS2D()
{
	NodeName = TEXT("LOS (2D horizontal)");
	Interval = 0.15f; RandomDeviation = 0.05f; // 6–8 Hz
}

void UBTS_LOS2D::TickNode(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!BB || !AI) return;

	const APawn* Me = AI->GetPawn();
	const AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Me || !Target)
	{
		BB->SetValueAsBool(HasLOSKey.SelectedKeyName, false);
		return;
	}

	float Dist = 0.f;
	const bool bHasLOS = HasLOS_2D(Me, Target, EyeHeight, SightRange, LOSChannel, Dist);

	BB->SetValueAsBool(HasLOSKey.SelectedKeyName, bHasLOS);

	if (!DistKey.SelectedKeyName.IsNone())
	{
		BB->SetValueAsFloat(DistKey.SelectedKeyName, Dist);
	}

	// --- Coop: broadcast ONLY when we *become* alerted ---

	const bool bHasAlertKeys =
		!AlertedKey.SelectedKeyName.IsNone() &&
		!AlertLocationKey.SelectedKeyName.IsNone();

	if (!bHasAlertKeys)
	{
		return;
	}

	// What was our alert state before this tick?
	const bool bWasAlerted = BB->GetValueAsBool(AlertedKey.SelectedKeyName);

	if (bHasLOS && !bWasAlerted)
	{
		// First time we see the player (or after a reset) -> alert + broadcast once
		const FVector AlertPos = Target->GetActorLocation();

		BB->SetValueAsBool(AlertedKey.SelectedKeyName, true);
		BB->SetValueAsVector(AlertLocationKey.SelectedKeyName, AlertPos);

		UE_LOG(LogTemp, Log,
			TEXT("[LOS2D] %s FIRST ALERT, broadcasting (Radius=%.1f)"),
			*Me->GetName(), AlertRadius);

		AlertAllies(Me, AlertPos, AlertRadius,
			AlertedKey.SelectedKeyName,
			AlertLocationKey.SelectedKeyName);
	}

	// If bHasLOS is true AND bWasAlerted is already true -> do nothing, no spam.
	// If bHasLOS is false, we also do nothing here; you can reset IsAlerted somewhere else if you want.
}

bool UBTS_LOS2D::HasLOS_2D(const APawn* Me, const AActor* Target, float EyeZ, float Range, ECollisionChannel Chan, float& OutDist)
{
	const FVector M = Me->GetActorLocation();
	const FVector T = Target->GetActorLocation();

	FVector Dir = T - M; Dir.Y = 0.f;     // 2D XZ
	OutDist = Dir.Size(); if (OutDist > Range) return false;
	if (OutDist <= KINDA_SMALL_NUMBER) return true;

	const FVector Start = FVector(M.X, M.Y, M.Z + EyeZ);
	const FVector End = Start + Dir.GetSafeNormal() * (OutDist + 1.f);
	FHitResult Hit; FCollisionQueryParams P(SCENE_QUERY_STAT(LOS2D), false, Me);
	if (!Me->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Chan, P)) return true;
	return Hit.GetActor() == Target;
}

void UBTS_LOS2D::AlertAllies(const APawn* Me, const FVector& AlertPos, float Radius, FName AlertedKeyName, FName AlertLocationKeyName)
{
	if (!Me) return;

	UWorld* World = Me->GetWorld();
	if (!World) return;

	const float RadiusSq = Radius * Radius;

	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsOfClass(World, Me->GetClass(), Candidates);

	int32 AlertCount = 0;

	for (AActor* Actor : Candidates)
	{
		if (!Actor || Actor == Me) continue;

		const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), AlertPos);
		if (DistSq > RadiusSq)
			continue;

		APawn* OtherPawn = Cast<APawn>(Actor);
		if (!OtherPawn) continue;

		AAIController* OtherAI = Cast<AAIController>(OtherPawn->GetController());
		if (!OtherAI) continue;

		UBlackboardComponent* OtherBB = OtherAI->GetBlackboardComponent();
		if (!OtherBB) continue;

		const FVector OtherLoc = OtherPawn->GetActorLocation();
		FVector LocalAlert = OtherLoc;
		LocalAlert.X = AlertPos.X;          // <-- only X from player

		OtherBB->SetValueAsBool(AlertedKeyName, true);
		OtherBB->SetValueAsVector(AlertLocationKeyName, LocalAlert);

		++AlertCount;

		UE_LOG(LogTemp, Log,
			TEXT("[LOS2D][AlertAllies] %s ALERTED %s (dist=%.1f, radius=%.1f)"),
			*Me->GetName(), *Actor->GetName(),
			FMath::Sqrt(DistSq), Radius);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[LOS2D][AlertAllies] %s broadcast done. Radius=%.1f, AlertedCount=%d, Candidates=%d"),
		*Me->GetName(), Radius, AlertCount, Candidates.Num());
}
