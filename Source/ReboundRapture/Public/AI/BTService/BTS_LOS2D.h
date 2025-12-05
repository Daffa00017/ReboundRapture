// BTService Line Of Sight 2D Header

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTS_LOS2D.generated.h"

/**
 * 
 */
UCLASS()
class REBOUNDRAPTURE_API UBTS_LOS2D : public UBTService
{
	GENERATED_BODY()

public:
	UBTS_LOS2D();

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetKey;     // AttackTarget
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector HasLOSKey;     // HasLOS (Bool)
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector DistKey;       // DistToTarget (Float, optional)
	// NEW: coop keys
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector AlertedKey;       // IsAlerted (Bool)
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector AlertLocationKey; // AlertLocation (Vector)

	UPROPERTY(EditAnywhere, Category = "Sense") float SightRange = 2000.f;
	UPROPERTY(EditAnywhere, Category = "Sense") float EyeHeight = 30.f;
	// NEW: how far the shout goes
	UPROPERTY(EditAnywhere, Category = "Coop") float AlertRadius = 1200.f;
	UPROPERTY(EditAnywhere, Category = "Sense") TEnumAsByte<ECollisionChannel> LOSChannel = ECC_Visibility;



protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	static bool HasLOS_2D(const APawn* Me, const AActor* Target, float EyeZ, float Range, ECollisionChannel Chan, float& OutDist);

	static void AlertAllies(const APawn* Me, const FVector& AlertPos, float Radius, FName AlertedKeyName, FName AlertLocationKeyName);
	
};
