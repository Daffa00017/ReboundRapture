// laneLevel Implementation


#include "Actor/LevelActor/LaneLevelGenerator.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "PaperSprite.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"


//--Helpers--

inline float LaneGridLeftX_Local(int32 NumLanes, float LaneWidthUU)
{
	return -0.5f * (float(NumLanes) * LaneWidthUU);
}

inline float LaneCenterX_Local(int32 Lane, int32 NumLanes, float LaneWidthUU)
{
	const float left = LaneGridLeftX_Local(NumLanes, LaneWidthUU);
	const int32 idx = (Lane % NumLanes + NumLanes) % NumLanes;
	return left + (idx + 0.5f) * LaneWidthUU;
}

// Sets default values
ALaneLevelGenerator::ALaneLevelGenerator()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	LeftSeam = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftSeam"));
	RightSeam = CreateDefaultSubobject<UBoxComponent>(TEXT("RightSeam"));
	LeftSeam->SetupAttachment(Root);
	RightSeam->SetupAttachment(Root);

	// seams collide as overlaps
	LeftSeam->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LeftSeam->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeftSeam->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RightSeam->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RightSeam->SetCollisionResponseToAllChannels(ECR_Ignore);
	RightSeam->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	LeftSeam->SetGenerateOverlapEvents(true);
	RightSeam->SetGenerateOverlapEvents(true);
	LeftSeam->OnComponentBeginOverlap.AddDynamic(this, &ALaneLevelGenerator::OnLeftSeamBeginOverlap);
	RightSeam->OnComponentBeginOverlap.AddDynamic(this, &ALaneLevelGenerator::OnRightSeamBeginOverlap);

	//  CREATE WALLS HERE (not in OnConstruction)
	LeftWall = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftWall"));
	RightWall = CreateDefaultSubobject<UBoxComponent>(TEXT("RightWall"));
	LeftWall->SetupAttachment(Root);
	RightWall->SetupAttachment(Root);

	auto SetupWall = [](UBoxComponent* B)
		{
			B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			B->SetCollisionResponseToAllChannels(ECR_Block);
			B->SetGenerateOverlapEvents(false);
		};
	SetupWall(LeftWall);
	SetupWall(RightWall);
}

void ALaneLevelGenerator::BeginPlay()
{
	Super::BeginPlay();
	RecomputePlayfield();
	UpdateSeamPosY();

	// cache wall sprite size if we have a reference
	WallTileWUU = WallTileHUU = 0.f;
	if (WallVariants.Num() > 0 && IsValid(WallVariants[0]))
	{
		const float ppuu = FMath::Max(WallVariants[0]->GetPixelsPerUnrealUnit(), 0.001f);
		const FVector2D px = WallVariants[0]->GetSourceSize();
		WallTileWUU = px.X / ppuu;
		WallTileHUU = px.Y / ppuu;
	}

	if (bShowWalls) InitWallsInfinite();

	ScaffoldLane = FMath::Clamp(NumLanes / 2, 0, NumLanes - 1);
}


void ALaneLevelGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RecomputePlayfield();
	UpdateSeamPosY();
	//RebuildWalls();
}

void ALaneLevelGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bSeamsFollowPlayerY) { UpdateSeamPosY(); }
	if (bDrawDebugSeams) { DrawSeamDebug(); }

	EnsureSegmentsAhead();   // NEW
	CullOldRows();           // NEW
	if (bDrawScaffoldDebug) { DrawScaffoldDebug(); } // NEW
	if (bShowWalls) UpdateWallsInfinite();
}

void ALaneLevelGenerator::RecomputePlayfield()
{
	// If a reference sprite is set, compute lane width = TilesPerLane * tileWUU
	if (ReferenceTileSprite)
	{
		const float PPUU = FMath::Max(ReferenceTileSprite->GetPixelsPerUnrealUnit(), 0.001f);
		const float TileWUU = ReferenceTileSprite->GetSourceSize().X / PPUU; // 16 / 0.15 = 106.666...
		LaneWidthUU = TileWUU * FMath::Max(1, TilesPerLane);        // e.g. TilesPerLane=2 -> 21.3333
	}

	PlayfieldWidthUU = FMath::Max(NumLanes, 1) * FMath::Max(LaneWidthUU, 1.f);
	Xmin = -0.5f * PlayfieldWidthUU;
	Xmax = +0.5f * PlayfieldWidthUU;

	SeamHalfHeightUU = SeamHalfHeightsInScreens * ScreenWorldHeightUU;

	// Local positions, not world:
	LeftSeam->SetRelativeLocation(FVector(Xmin, 0.f, 0.f));
	RightSeam->SetRelativeLocation(FVector(Xmax, 0.f, 0.f));

	// Box extents are in local space; X thin, Y tall (local “vertical”), Z small
	const FVector Extents(SeamHalfThicknessX, SeamHalfHeightUU, 100.f);
	LeftSeam->SetBoxExtent(Extents);
	RightSeam->SetBoxExtent(Extents);
}

void ALaneLevelGenerator::UpdateSeamPosY()
{
	float LocalY = 0.f;
	if (bSeamsFollowPlayerY && PlayerRef)
	{
		const FTransform T = GetActorTransform();
		const FVector PlayerLocal = T.InverseTransformPosition(PlayerRef->GetActorLocation());
		LocalY = PlayerLocal.Y; // follow along the actor’s local +Y (ForwardVector)
	}
	// keep X at +/-W/2, update local Y
	LeftSeam->SetRelativeLocation(FVector(Xmin, LocalY, 0.f));
	RightSeam->SetRelativeLocation(FVector(Xmax, LocalY, 0.f));
}

bool ALaneLevelGenerator::IsEligible(AActor* A) const
{
	if (!A) return false;

	// Always allow the explicit player ref
	if (A == PlayerRef) return true;

	// Tag-based eligibility
	if (EligibleTag != NAME_None && A->ActorHasTag(EligibleTag))
		return true;

	// Optionally: any Pawn
	if (bWrapPawnsByDefault && A->IsA<APawn>())
		return true;

	return false;
}

void ALaneLevelGenerator::TryWrap(AActor* A, bool bFromLeftSeam)
{
	if (!A) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (float* Until = CooldownUntil.Find(A))
		if (Now < *Until) return;

	const float W = PlayfieldWidthUU;
	const FVector Right = GetActorRightVector(); // local +X
	const FVector Inset = Right * InsetUU;
	const FVector Shift = Right * W;

	FVector L = A->GetActorLocation();
	if (bFromLeftSeam)
		L = L + Shift - Inset;   // to right side, nudge inward
	else
		L = L - Shift + Inset;   // to left side, nudge inward

	FHitResult Hit;
	A->SetActorLocation(L, /*bSweep=*/true, &Hit, ETeleportType::TeleportPhysics);

	CooldownUntil.FindOrAdd(A) = Now + WrapCooldownSeconds;
}

void ALaneLevelGenerator::OnLeftSeamBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (IsEligible(OtherActor))
	{
		TryWrap(OtherActor, /*bFromLeftSeam=*/true);
	}
}

void ALaneLevelGenerator::OnRightSeamBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (IsEligible(OtherActor))
	{
		TryWrap(OtherActor, /*bFromLeftSeam=*/false);
	}
}

void ALaneLevelGenerator::DrawSeamDebug() const
{
	if (!GetWorld()) return;

	// --- existing seam lines ---
	const FVector Lc = LeftSeam->GetComponentLocation();
	const FVector Rc = RightSeam->GetComponentLocation();
	const FVector UpLocalY = GetActorForwardVector();  // local +Y
	const float   HalfH = SeamHalfHeightUU;
	const FVector Dy = UpLocalY * HalfH;

	// seams (cyan)
	DrawDebugLine(GetWorld(), Lc - Dy, Lc + Dy, FColor::Cyan, false, 0.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), Rc - Dy, Rc + Dy, FColor::Cyan, false, 0.f, 0, 1.5f);

	// --- lane center guides ---
	if (bDrawLaneGuides)
	{
		const FTransform T = GetActorTransform();
		const float BandCenterLocalY = T.InverseTransformPosition(Lc).Y; // left/right share same Y

		for (int32 Lane = 0; Lane < NumLanes; ++Lane)
		{
			const float Xl = LaneCenterX_Local(Lane, NumLanes, LaneWidthUU); // <-- updated
			const FVector A_local(Xl, BandCenterLocalY - HalfH, 0);
			const FVector B_local(Xl, BandCenterLocalY + HalfH, 0);

			const FVector A = T.TransformPosition(A_local);
			const FVector B = T.TransformPosition(B_local);

			DrawDebugLine(GetWorld(), A, B, FColor::Red, false, 0.f, 0, 0.8f);
		}
	}
}

void ALaneLevelGenerator::EnsureSegmentsAhead()
{
	if (ScreenWorldHeightUU <= KINDA_SMALL_NUMBER) return;

	const float targetLocalY = PlayerLocalY() + SpawnLeadScreens * ScreenWorldHeightUU;

	const int32 cap = FMath::Clamp(Gen_MaxRowsPerTick, 0, 32);
	int32 built = 0;

	while (CursorLocalY + 0.1f < targetLocalY && built < cap)
	{
		GenerateScaffoldSegment();
		++built;
		if (bGen_DryRun_NoSpawn)
		{
			// If we’re dry-running, CursorLocalY still advances inside GenerateScaffoldSegment.
			// Nothing else to do here.
		}
	}
}

void ALaneLevelGenerator::CullOldRows()
{
	const float CullY = PlayerLocalY() - CullBufferScreens * ScreenWorldHeightUU; // local +Y is your row axis
	for (int32 i = LiveRows.Num() - 1; i >= 0; --i)
	{
		if (LiveRows[i].LocalY < CullY)
		{
			DespawnRow(LiveRows[i]);
			LiveRows.RemoveAt(i);
		}
	}
}

void ALaneLevelGenerator::GenerateScaffoldSegment()
{
	// Occasionally turn the scaffold lane a step left/right (keeps the “path” alive)
	if (FMath::FRand() < TurnChance())
	{
		const int dir = (FMath::FRand() < 0.5f) ? -1 : +1;
		ScaffoldLane = (ScaffoldLane + dir + NumLanes) % NumLanes;
	}

	// Build a row mask: scaffold + extras (supports >8 lanes via uint16)
	const uint16 Mask0 = BuildRowMask_WithExtras(ScaffoldLane);

	// Validate against previous row for reachability; reroll a few times if needed
	uint16 PrevMask = 0;
	if (LiveRows.Num() > 0) PrevMask = LiveRows.Last().ScaffoldBits;

	uint16 FinalMask = Mask0;
	int Reroll = 0;
	while (!ValidateRowMask(PrevMask, FinalMask) && Reroll < RowRerollAttempts)
	{
		FinalMask = BuildRowMask_WithExtras(ScaffoldLane);
		++Reroll;
	}

	// Record this row (even in dry-run; CursorLocalY still advances)
	FRowBit Row;
	Row.ScaffoldBits = FinalMask;
	Row.RowIndex = NextRowIndex++;
	Row.LocalY = CursorLocalY;

	// Isolation modes:
	// - If SkipRuns: keep the legacy single-lane scaffold for testing
	// - Else: build runs (and hazards if enabled), then spawn
	if (!bGen_SkipRuns && !bGen_DryRun_NoSpawn)
	{
		// Runs
		TArray<FRowRun> Runs;
		BuildRunsFromMask(FinalMask, ScaffoldLane, Runs);

		// Hazards (behind a flag so you can test “runs-only” first)
		if (!bGen_SkipHazards)
		{
			ApplyHazardsToRuns(PrevMask, FinalMask, Runs);
		}

		// Spawn rows of platforms at this LocalY
		SpawnRuns(Runs, Row.LocalY, Row.Actors);
	}
	else if (!bGen_DryRun_NoSpawn && bGen_SkipRuns)
	{
		// Keep your old isolation path (one guaranteed safe lane)
		GenerateScaffoldSegment_Simple();
		return;
	}

	// Bookkeeping: keep the row and advance the cursor downward by one row height
	LiveRows.Add(Row); // NOTE: if your member is named LiveRows (usual), use that. If you see a typo here, correct to LiveRows.
	CursorLocalY += RowHeightUU;
}

void ALaneLevelGenerator::DrawScaffoldDebug()
{
	if (!GetWorld()) return;

	const float boxW = LaneWidthUU * 0.80f;
	const float boxH = RowHeightUU * 0.25f;
	const FVector extents(boxW * 0.5f, boxH * 0.5f, 8.f);

	for (const FRowBit& Row : LiveRows)
	{
		TArray<FRowRun> Runs;
		BuildRunsFromMask(Row.ScaffoldBits, ScaffoldLane, Runs);

		for (const FRowRun& R : Runs)
		{
			const float leftCenterX = LaneCenterX_Local(R.StartLane, NumLanes, LaneWidthUU);
			const float centerX = leftCenterX + 0.5f * float(R.LenLanes - 1) * LaneWidthUU;

			const FVector localCenter(centerX, Row.LocalY, 0.f);
			const FVector worldCenter = GetActorTransform().TransformPosition(localCenter);

			DrawDebugBox(GetWorld(), worldCenter, extents, FQuat::Identity,
				R.bScaffold ? FColor::Cyan : FColor::Silver,
				false, 0.f, 0, 0.9f);
		}
	}
}

void ALaneLevelGenerator::GenerateScaffoldSegment_Simple()
{
	if (FMath::FRand() < TurnChance())
	{
		const int dir = (FMath::FRand() < 0.5f) ? -1 : +1;
		ScaffoldLane = (ScaffoldLane + dir + NumLanes) % NumLanes;
	}

	FRowBit row;
	row.ScaffoldBits = (uint16(1) << ScaffoldLane);
	row.RowIndex = NextRowIndex++;
	row.LocalY = CursorLocalY;

	if (ScaffoldPlatformClass && !bGen_DryRun_NoSpawn)
	{
		const int32 tiles = FMath::Max(TilesPerLane, 1);

		const float centerX = LaneCenterX_Local(ScaffoldLane, NumLanes, LaneWidthUU);
		FVector worldPos = GetActorTransform().TransformPosition(FVector(centerX, row.LocalY, 0.f));
		if (bLockPlatformsToPlayerY && PlayerRef) worldPos.Y = PlayerRef->GetActorLocation().Y;
		else                                       worldPos.Y = PlatformsY;

		const FRotator worldRot = FRotator::ZeroRotator;

		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.Owner = this;

		if (APlatformStrip* Plat = GetWorld()->SpawnActor<APlatformStrip>(ScaffoldPlatformClass, worldPos, worldRot, P))
		{
			Plat->SetVisualKind(EPlatformKind::Solid);
			Plat->SetSpriteRollDegrees(PlatformSpriteRollDeg);

			// base thickness (+Y) before pads/boost
			Plat->CollisionHeightUU_Override = (PlatformCollisionHeightUU > 0.f) ? PlatformCollisionHeightUU : -1.f;

			// show real collider if desired
			Plat->SetCollisionVisible(bRevealPlatformCollision);

			// pads/boost from generator (extend L/R, thicken F/B, lift top only)
			Plat->SetCollisionPads(PlatformCollisionPadXUU, PlatformCollisionPadYUU, PlatformCollisionTopBoostUU);

			// build the collider-only strip (fallback path); tiling path also calls ApplyCollisionSizing
			Plat->BuildDebugFallback(tiles);
			row.Actors.Add(Plat);
		}
	}

	LiveRows.Add(row);
	CursorLocalY += RowHeightUU;
}

void ALaneLevelGenerator::InitWallsInfinite()
{
	// wipe old
	for (auto* C : WallSpritesLeft)  if (C) C->DestroyComponent();
	for (auto* C : WallSpritesRight) if (C) C->DestroyComponent();
	WallSpritesLeft.Empty();
	WallSpritesRight.Empty();

	if (WallTileHUU <= 0.f || WallVariants.Num() == 0) return;

	// pick a valid ref
	UPaperSprite* Ref = nullptr;
	for (auto* S : WallVariants) if (IsValid(S)) { Ref = S; break; }
	if (!Ref) return;

	const float bandHalfUU = WallsHalfScreensVisible * ScreenWorldHeightUU;

	// center the initial band around current player Y (or 0 if none)
	const float centerLocalY = (PlayerRef)
		? GetActorTransform().InverseTransformPosition(PlayerRef->GetActorLocation()).Y
		: 0.f;

	// initial Y coverage
	const float startY = centerLocalY - bandHalfUU - WallVerticalTilesPadding * WallTileHUU;
	const float endY = centerLocalY + bandHalfUU + WallVerticalTilesPadding * WallTileHUU;

	// helper spawner for one tile at (X,Y)
	auto SpawnTile = [&](TArray<UPaperSpriteComponent*>& Out, float XLocal, float YLocal, bool bRight)
		{
			// pick a variant (skip nulls)
			UPaperSprite* Pick = nullptr;
			for (int t = 0; t < 8 && !Pick; ++t)
			{
				Pick = WallVariants[FMath::RandHelper(WallVariants.Num())];
				if (!IsValid(Pick)) Pick = nullptr;
			}
			if (!Pick) Pick = Ref;

			auto* S = NewObject<UPaperSpriteComponent>(this);
			S->RegisterComponent();
			S->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			S->SetSprite(Pick);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			// face camera (+Z); roll matches your art orientation
			S->SetRelativeRotation(FRotator(0.f, 0.f, WallSpriteRollDeg));
			if (bRight && bMirrorRightColumn) S->SetRelativeScale3D(FVector(-1.f, 1.f, 1.f));

			S->SetRelativeLocation(FVector(XLocal, YLocal, 0.f));
			Out.Add(S);
		};

	const float leftX = Xmin + WallInsetX;
	const float rightX = Xmax - WallInsetX;

	// fill the initial band
	float y = startY;
	while (y <= endY + 0.5f * WallTileHUU)
	{
		SpawnTile(WallSpritesLeft, leftX, y, /*bRight=*/false);
		SpawnTile(WallSpritesRight, rightX, y, /*bRight=*/true);
		y += WallTileHUU;
	}

	// place/size blockers on art face
	const float halfW = 0.5f * WallTileWUU;
	const float faceLeftX = leftX + halfW - WallFaceContactInsetUU;
	const float faceRightX = rightX - halfW + WallFaceContactInsetUU;

	const FVector halfExtents(WallBlockThicknessUU * 0.5f, bandHalfUU, 2000.f);
	if (LeftWall) { LeftWall->SetBoxExtent(halfExtents);  LeftWall->SetRelativeLocation(FVector(faceLeftX, centerLocalY, 0.f)); }
	if (RightWall) { RightWall->SetBoxExtent(halfExtents); RightWall->SetRelativeLocation(FVector(faceRightX, centerLocalY, 0.f)); }

	// track range
	WallTopY = startY;
	WallNextSpawnY = y;         // first Y below the seeded band
}

void ALaneLevelGenerator::UpdateWallsInfinite()
{
	if (WallTileHUU <= 0.f) return;

	// where the player is (decides when to add/cull), but we DO NOT move existing tiles
	const float playerY = (PlayerRef)
		? GetActorTransform().InverseTransformPosition(PlayerRef->GetActorLocation()).Y
		: 0.f;

	const float leftX = Xmin + WallInsetX;
	const float rightX = Xmax - WallInsetX;

	// ---- Append below until we’re WallSpawnLeadScreens ahead ----
	const float wantBottomY = playerY + WallSpawnLeadScreens * ScreenWorldHeightUU;

	auto AppendPairAt = [&](float y)
		{
			// Left
			{
				UPaperSprite* Pick = nullptr;
				for (int t = 0; t < 8 && !Pick; ++t)
				{
					Pick = WallVariants[FMath::RandHelper(WallVariants.Num())];
					if (!IsValid(Pick)) Pick = nullptr;
				}
				if (!Pick && WallVariants.Num() > 0) Pick = WallVariants[0];

				auto* S = NewObject<UPaperSpriteComponent>(this);
				S->RegisterComponent();
				S->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
				S->SetSprite(Pick);
				S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				S->SetRelativeRotation(FRotator(0.f, 0.f, WallSpriteRollDeg));
				S->SetRelativeLocation(FVector(leftX, y, 0.f));
				WallSpritesLeft.Add(S);
			}
			// Right
			{
				UPaperSprite* Pick = nullptr;
				for (int t = 0; t < 8 && !Pick; ++t)
				{
					Pick = WallVariants[FMath::RandHelper(WallVariants.Num())];
					if (!IsValid(Pick)) Pick = nullptr;
				}
				if (!Pick && WallVariants.Num() > 0) Pick = WallVariants[0];

				auto* S = NewObject<UPaperSpriteComponent>(this);
				S->RegisterComponent();
				S->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
				S->SetSprite(Pick);
				S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				S->SetRelativeRotation(FRotator(0.f, 0.f, WallSpriteRollDeg));
				if (bMirrorRightColumn) S->SetRelativeScale3D(FVector(-1.f, 1.f, 1.f));
				S->SetRelativeLocation(FVector(rightX, y, 0.f));
				WallSpritesRight.Add(S);
			}
		};

	int guard = 0;
	while (WallNextSpawnY < wantBottomY && guard++ < 512)
	{
		AppendPairAt(WallNextSpawnY);
		WallNextSpawnY += WallTileHUU;
	}

	// ---- Cull far above to save perf/mem ----
	const float cullAboveY = playerY - WallCullAboveScreens * ScreenWorldHeightUU;

	while (WallSpritesLeft.Num() > 0 &&
		WallSpritesLeft[0] &&
		(WallSpritesLeft[0]->GetRelativeLocation().Y + WallTileHUU) < cullAboveY)
	{
		if (WallSpritesLeft[0])  WallSpritesLeft[0]->DestroyComponent();
		if (WallSpritesRight[0]) WallSpritesRight[0]->DestroyComponent();
		WallSpritesLeft.RemoveAt(0);
		WallSpritesRight.RemoveAt(0);
		WallTopY += WallTileHUU;
	}

	// ---- Keep blocker boxes centered on the visible band (so they always block) ----
	const float halfW = 0.5f * WallTileWUU;
	const float faceLeftX = leftX + halfW - WallFaceContactInsetUU;
	const float faceRightX = rightX - halfW + WallFaceContactInsetUU;

	const float bandHalfUU = WallsHalfScreensVisible * ScreenWorldHeightUU;
	const float bandCenterY = playerY; // use player for the blocking span only

	if (LeftWall) { LeftWall->SetRelativeLocation(FVector(faceLeftX, bandCenterY, 0.f));  LeftWall->SetBoxExtent(FVector(WallBlockThicknessUU * 0.5f, bandHalfUU, 2000.f)); }
	if (RightWall) { RightWall->SetRelativeLocation(FVector(faceRightX, bandCenterY, 0.f)); RightWall->SetBoxExtent(FVector(WallBlockThicknessUU * 0.5f, bandHalfUU, 2000.f)); }
}

FVector ALaneLevelGenerator::RowCenterWorld(int32 Lane, float LocalY) const
{
	const float Xl = LaneCenterX_Local(Lane, NumLanes, LaneWidthUU); // <-- updated
	const FVector Local(Xl, LocalY, 0.f);
	return GetActorTransform().TransformPosition(Local);
}

uint16 ALaneLevelGenerator::BuildRowMask_WithExtras(int32 ScaffoldLaneIdx) const
{
	const int32 lanes = FMath::Max(NumLanes, 1);
	const uint16 one = 1;
	uint16 mask = (one << (ScaffoldLaneIdx % lanes));  // safe even if lanes > 8

	int32 extras = 0;
	const float p = ExtraPlatformChance();
	for (int i = 0; i < lanes && extras < MaxExtrasPerRow; ++i)
	{
		if (i == ScaffoldLaneIdx) continue;
		if (FMath::FRand() < p) { mask |= (one << i); ++extras; }
	}
	return mask;
}

bool ALaneLevelGenerator::ValidateRowMask(uint16 PrevMask, uint16 ThisMask) const
{
	if (PrevMask == 0) return true;

	auto BitSet = [](uint16 m, int i) { return (m & (uint16(1) << i)) != 0; };

	for (int lane = 0; lane < NumLanes; ++lane)
	{
		if (!BitSet(ThisMask, lane)) continue;

		bool ok = false;
		for (int d = -MaxGapLanes; d <= MaxGapLanes && !ok; ++d)
		{
			const int prev = (lane + d + NumLanes) % NumLanes; // wrap ok
			if (BitSet(PrevMask, prev)) ok = true;
		}
		if (!ok) return false;
	}
	return true;
}

void ALaneLevelGenerator::BuildRunsFromMask(uint16 Mask, int32 InScaffoldLane, TArray<FRowRun>& OutRuns) const
{
	OutRuns.Reset();
	const int32 Lanes = FMath::Max(NumLanes, 1);
	auto Bit = [&](int i) { return (Mask & (uint16(1) << i)) != 0; };

	int lane = 0;
	while (lane < Lanes)
	{
		if (!Bit(lane)) { ++lane; continue; }

		const int start = lane;
		int len = 0;
		while (lane < Lanes && Bit(lane)) { ++len; ++lane; }

		FRowRun R;
		R.StartLane = start;
		R.LenLanes = FMath::Max(len, 1);
		R.bScaffold = (InScaffoldLane >= start && InScaffoldLane < start + len);
		R.Kind = ERunKind::Solid; // default; hazards may override
		OutRuns.Add(R);
	}
}

void ALaneLevelGenerator::DespawnRow(FRowBit& Row)
{
	for (TWeakObjectPtr<APlatformStrip>& W : Row.Actors)
		if (APlatformStrip* A = W.Get())
			A->Destroy();
	Row.Actors.Empty();
}

void ALaneLevelGenerator::ApplyHazardsToRuns(uint16 MaskAbove, uint16 MaskThis, TArray<FRowRun>& Runs) const
{
	const float pBreak = BreakableRatio();   // your curve
	const float pSpike = SpikeRatio();       // your curve

	auto laneHas = [&](uint16 m, int lane)
		{
			const int idx = (lane + NumLanes) % NumLanes;
			return (m & (uint16(1) << idx)) != 0;
		};

	// 1) Roll spike first, then breakable, for NON-scaffold
	for (FRowRun& R : Runs)
	{
		R.Kind = ERunKind::Solid;

		if (!R.bScaffold)
		{
			// Spike preference (e.g., if row above is empty over this lane)
			bool anySpike = false;
			for (int i = 0; i < R.LenLanes; ++i)
			{
				const int lane = (R.StartLane + i) % NumLanes;
				const bool noAbove = !laneHas(MaskAbove, lane);
				const bool spikeRoll = FMath::FRand() < pSpike;
				if (spikeRoll && noAbove) { anySpike = true; break; }
			}
			if (anySpike)
			{
				R.Kind = ERunKind::SpikeTop;
				continue;
			}

			// Breakable roll
			if (FMath::FRand() < pBreak)
			{
				R.Kind = ERunKind::Breakable;
			}
		}
	}

	// 2) Prevent scaffold from becoming fully non-solid by accident
	if (bKeepOneSolidOnScaffold)
	{
		for (FRowRun& R : Runs)
		{
			if (R.bScaffold && R.Kind != ERunKind::Solid)
			{
				R.Kind = ERunKind::Solid;
				break;
			}
		}
	}
}

void ALaneLevelGenerator::SpawnRuns(const TArray<FRowRun>& Runs, 
									float LocalY, 
									TArray<TWeakObjectPtr<class APlatformStrip>>& OutActors)
{
	if (bGen_DryRun_NoSpawn || !ScaffoldPlatformClass) return;

	UWorld* W = GetWorld();
	if (!W) { UE_LOG(LogTemp, Error, TEXT("SpawnRuns: World is null")); return; }

	const int32 lanes = FMath::Max(NumLanes, 1);

	for (const FRowRun& R : Runs)
	{
		const int32 len = FMath::Clamp(R.LenLanes, 1, lanes);
		const int32 tilesWide = FMath::Max(len, 1);

		// desired local center (same as debug)
		const float leftCenterX = LaneCenterX_Local(R.StartLane, lanes, LaneWidthUU);
		const float centerX = leftCenterX + 0.5f * float(len - 1) * LaneWidthUU;

		FVector localPos(centerX, LocalY, 0.f);
		if (bLockPlatformsToPlayerY && PlayerRef)
		{
			localPos.Y = GetTransform().InverseTransformPositionNoScale(PlayerRef->GetActorLocation()).Y;
		}

		const FTransform& Axf = GetActorTransform();
		const FVector worldPos = Axf.TransformPosition(localPos);
		const FRotator worldRot = FRotator::ZeroRotator;

		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.Owner = this;

		APlatformStrip* Plat = W->SpawnActor<APlatformStrip>(ScaffoldPlatformClass, worldPos, worldRot, P);
		if (!Plat) { UE_LOG(LogTemp, Warning, TEXT("SpawnRuns: failed to spawn APlatformStrip")); continue; }

		// visuals / tuning
		switch (R.Kind)
		{
		default:
		case ERunKind::Solid:     Plat->SetVisualKind(EPlatformKind::Solid);     break;
		case ERunKind::Breakable: Plat->SetVisualKind(EPlatformKind::Breakable); break;
		case ERunKind::SpikeTop:  Plat->SetVisualKind(EPlatformKind::SpikeTop);  break;
		}
		Plat->SetSpriteRollDegrees(PlatformSpriteRollDeg);
		Plat->CollisionHeightUU_Override = (PlatformCollisionHeightUU > 0.f) ? PlatformCollisionHeightUU : -1.f;

		Plat->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);

		// build
		if (bUsePlatformSafeMode) Plat->BuildDebugFallback(tilesWide);
		else                      Plat->BuildTiledByCount(tilesWide);

		// ---------- Clamp against walls (after build; uses real collider width) ----------
		{
			// platform half width in UU (collider, not sprite)
			const float halfX = Plat->GetCollisionHalfExtentX();

			// compute the wall *face* positions we should stay inside
			float faceLeftX = Xmin;  // fallback if no wall sprite data
			float faceRightX = Xmax;

			if (WallTileWUU > 0.f) // you cache this in BeginPlay()
			{
				const float leftColX = Xmin + WallInsetX;
				const float rightColX = Xmax - WallInsetX;
				const float halfWallW = 0.5f * WallTileWUU;

				faceLeftX = leftColX + halfWallW - WallFaceContactInsetUU;
				faceRightX = rightColX - halfWallW + WallFaceContactInsetUU;
			}

			const float minCenterX = faceLeftX + halfX + PlatformWallClearanceUU;
			const float maxCenterX = faceRightX - halfX - PlatformWallClearanceUU;

			// current local X of the actor
			const FVector curLocal = Axf.InverseTransformPosition(Plat->GetActorLocation());
			float clampedX = FMath::Clamp(curLocal.X, minCenterX, maxCenterX);

			if (!FMath::IsNearlyEqual(clampedX, curLocal.X, 0.1f))
			{
				const FVector newWorld = Axf.TransformPosition(FVector(clampedX, curLocal.Y, curLocal.Z));
				Plat->SetActorLocation(newWorld, /*bSweep=*/false);
			}
		}
		// -------------------------------------------------------------------------------

		OutActors.Add(Plat);
	}
}
