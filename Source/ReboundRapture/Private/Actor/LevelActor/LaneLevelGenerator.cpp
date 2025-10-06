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


	if (bSpawnPlatforms)
	{
		if (PlatformSpawnStartTime == 0.f)  // Only set this once when spawn starts
		{
			PlatformSpawnStartTime = GetWorld()->GetTimeSeconds();
		}

		// Check if the delay has passed
		if (GetWorld()->GetTimeSeconds() - PlatformSpawnStartTime >= PlatformSpawnDelay)
		{
			if (bFirstPlatformSpawn)
			{
				// Spawn first platform below the player
				if (PlayerRef)
				{
					// Adjust spawn position to be below the player
					CursorLocalY = PlayerLocalY() + PlatformSpawnOffsetY;
					bFirstPlatformSpawn = false; // Stop doing this after the first platform
				}
			}

			EnsureSegmentsAhead();   // Spawn platforms after the delay
			CullOldRows();           // Clean up old platforms
		}
	}

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

	// Previous row mask (for reachability + anti-stacking + "no back-to-back empty")
	const uint16 PrevMask = (LiveRows.Num() > 0) ? LiveRows.Last().ScaffoldBits : 0;

	// --- Row skipping (creates vertical gaps) ---
	// Reuse the platforms density ramp: when extras are rare, allow more empty rows.
	// We also avoid two empty rows in a row (PrevMask==0 guard).
	const float pExtra = ExtraPlatformChance();                 // 0..1 from your existing ramp
	const float rowSkipChance = FMath::Clamp(0.35f - 0.5f * pExtra, 0.f, 0.35f);
	const bool  bCanSkip = (PrevMask != 0);                     // don't create two blanks in a row
	const bool  bSkipThisRow = bCanSkip && (FMath::FRand() < rowSkipChance);

	// Row record (we always advance CursorLocalY)
	FRowBit Row;
	Row.RowIndex = NextRowIndex++;
	Row.LocalY = CursorLocalY;

	uint16 FinalMask = 0;

	if (!bSkipThisRow)
	{
		// Build: scaffold + extras, with anti-stack bias vs previous row
		const uint16 Mask0 = BuildRowMask_WithExtras(ScaffoldLane, PrevMask);

		// Validate against previous row for reachability; reroll a few times if needed
		FinalMask = Mask0;
		int Reroll = 0;
		while (!ValidateRowMask(PrevMask, FinalMask) && Reroll < RowRerollAttempts)
		{
			FinalMask = BuildRowMask_WithExtras(ScaffoldLane, PrevMask);
			++Reroll;
		}

		if (!bGen_SkipRuns && !bGen_DryRun_NoSpawn)
		{
			// Runs
			TArray<FRowRun> Runs;
			BuildRunsFromMask(FinalMask, ScaffoldLane, Runs);

			// Hazards behind a flag
			if (!bGen_SkipHazards)
			{
				ApplyHazardsToRuns(PrevMask, FinalMask, Runs);
			}

			// Spawn rows of platforms at this LocalY
			SpawnRuns(Runs, Row.LocalY, Row.Actors);
		}
		else if (!bGen_DryRun_NoSpawn && bGen_SkipRuns)
		{
			GenerateScaffoldSegment_Simple();
			return;
		}
	}

	// Bookkeeping + advance
	Row.ScaffoldBits = FinalMask;   // 0 if the row was skipped
	LiveRows.Add(Row);
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

uint16 ALaneLevelGenerator::BuildRowMask_WithExtras(int32 ScaffoldLaneIdx, uint16 PrevMask) const
{
	const int32 lanes = FMath::Max(NumLanes, 1);
	const uint16 one = 1;

	// Always include scaffold lane
	uint16 mask = (one << (ScaffoldLaneIdx % lanes));

	// Base probability for extras from your existing ramp
	const float pBase = ExtraPlatformChance();

	// Simple anti-stack: if previous row had a platform in this lane, reduce chance this row
	auto LaneHadPrev = [&](int i)
		{
			const int idx = (i + lanes) % lanes;
			return (PrevMask & (uint16(1) << idx)) != 0;
		};

	int32 extras = 0;
	for (int i = 0; i < lanes && extras < MaxExtrasPerRow; ++i)
	{
		if (i == ScaffoldLaneIdx) continue;

		// Bias away from stacking: reduce probability if lane was used in PrevMask
		const float p = LaneHadPrev(i) ? (pBase * 0.35f) : pBase;

		if (FMath::FRand() < p)
		{
			mask |= (one << i);
			++extras;
		}
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

	// Reference tile width in UU (derived from lane width and TilesPerLane)
	const int32 safeTPL = FMath::Max(TilesPerLane, 1);
	const float tileWUU = LaneWidthUU / float(safeTPL);

	// Plan lengths per run first (so we can decide “only one if >6”)
	struct FPlanned
	{
		int32 Index = 0;        // index into Runs
		int32 TilesWide = 2;    // chosen (capped) tiles
		int32 LenLanes = 1;     // run span in lanes
		bool  bScaffold = false;
	};

	TArray<FPlanned> Planned;
	Planned.Reserve(Runs.Num());

	int32 idx = 0;
	for (const FRowRun& R : Runs)
	{
		const int32 len = FMath::Clamp(R.LenLanes, 1, lanes);

		// Random length, then cap to fit inside this run (avoid overlap with neighbour runs)
		const int32 rnd = FMath::Clamp(FMath::RandRange(MinTilesPerStrip, MaxTilesPerStrip), 2, 128);
		const int32 maxTilesThatFit = len * safeTPL; // e.g. 2 lanes * 2 tilesPerLane = 4 tiles across
		const int32 tilesWide = FMath::Clamp(rnd, 2, FMath::Max(2, maxTilesThatFit));

		FPlanned P;
		P.Index = idx++;
		P.TilesWide = tilesWide;
		P.LenLanes = len;
		P.bScaffold = R.bScaffold;
		Planned.Add(P);
	}

	// Conflict detection: Check if two platforms land in the same lane
	TMap<int32, TArray<int32>> LanePlatformsMap; // Maps lane -> platform index

	for (int i = 0; i < Planned.Num(); ++i)
	{
		const FPlanned& P = Planned[i];
		const FRowRun& R = Runs[P.Index];

		// Track platforms per lane
		for (int j = R.StartLane; j < R.StartLane + P.LenLanes; ++j)
		{
			LanePlatformsMap.FindOrAdd(j).Add(i);
		}
	}

	// Now, assign random platform types if more than 1 platform lands in the same lane
	for (int i = 0; i < Planned.Num(); ++i)
	{
		const FPlanned& P = Planned[i];
		const FRowRun& R = Runs[P.Index];

		// Get the lanes this platform occupies
		bool bHasConflict = false;
		for (int j = R.StartLane; j < R.StartLane + P.LenLanes; ++j)
		{
			if (LanePlatformsMap[j].Num() > 1)
			{
				bHasConflict = true;
				break;
			}
		}

		// If conflict: Randomly change the platform type in the Planned array, ensuring they are different
		if (bHasConflict)
		{
			// Randomly choose a new type for the conflicting platform (make sure they're different)
			int32 newType = FMath::RandRange(2, 6);  // Random between 2 and 6 tiles

			// Ensure that if we already had a platform with this type, we pick a different type
			while (newType == P.TilesWide)
			{
				newType = FMath::RandRange(2, 6);
			}

			Planned[i].TilesWide = newType;  // Assign the new type
		}
	}

	// Now spawn the platforms as usual
	for (int32 pi = 0; pi < Planned.Num(); ++pi)
	{
		const FPlanned& P = Planned[pi];
		const FRowRun& R = Runs[P.Index];

		// placement: center of the run (same as your debug boxes)
		const float leftCenterX = LaneCenterX_Local(R.StartLane, lanes, LaneWidthUU);
		const float centerX = leftCenterX + 0.5f * float(P.LenLanes - 1) * LaneWidthUU;

		FVector localPos(centerX, LocalY, 0.f);
		if (bLockPlatformsToPlayerY && PlayerRef)
		{
			localPos.Y = GetTransform().InverseTransformPositionNoScale(PlayerRef->GetActorLocation()).Y;
		}

		const FTransform& Axf = GetActorTransform();
		const FVector worldPos = Axf.TransformPosition(localPos);
		const FRotator worldRot = FRotator::ZeroRotator;

		FActorSpawnParameters Psp;
		Psp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Psp.Owner = this;

		APlatformStrip* Plat = W->SpawnActor<APlatformStrip>(ScaffoldPlatformClass, worldPos, worldRot, Psp);
		if (!Plat) { UE_LOG(LogTemp, Warning, TEXT("SpawnRuns: failed to spawn APlatformStrip")); continue; }

		// visuals/hazards
		switch (R.Kind)
		{
		default:
		case ERunKind::Solid:     Plat->SetVisualKind(EPlatformKind::Solid);     break;
		case ERunKind::Breakable: Plat->SetVisualKind(EPlatformKind::Breakable); break;
		case ERunKind::SpikeTop:  Plat->SetVisualKind(EPlatformKind::SpikeTop);  break;
		}
		Plat->SetSpriteRollDegrees(PlatformSpriteRollDeg);
		Plat->CollisionHeightUU_Override = (PlatformCollisionHeightUU > 0.f) ? PlatformCollisionHeightUU : -1.f;

		// pads/top-anchor etc. (if you added those setters earlier)
		Plat->SetCollisionPads(PlatformCollisionPadXUU, PlatformCollisionPadYUU, PlatformCollisionTopBoostUU);
		Plat->SetCollisionVisible(bRevealPlatformCollision);

		Plat->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);

		// build the exact width (flex)
		if (bUsePlatformSafeMode)
			Plat->BuildDebugFallback(P.TilesWide);
		else
			Plat->BuildTiledByCount_Flex(P.TilesWide);

		// ---- Clamp against walls (uses true collider half width) ----
		{
			const float halfX = Plat->GetCollisionHalfExtentX();

			float faceLeftX = Xmin;
			float faceRightX = Xmax;

			if (WallTileWUU > 0.f)
			{
				const float leftColX = Xmin + WallInsetX;
				const float rightColX = Xmax - WallInsetX;
				const float halfWallW = 0.5f * WallTileWUU;
				faceLeftX = leftColX + halfWallW - WallFaceContactInsetUU;
				faceRightX = rightColX - halfWallW + WallFaceContactInsetUU;
			}

			const float minCenterX = faceLeftX + halfX + PlatformWallClearanceUU;
			const float maxCenterX = faceRightX - halfX - PlatformWallClearanceUU;

			const FVector curLocal = Axf.InverseTransformPosition(Plat->GetActorLocation());
			const float clampedX = FMath::Clamp(curLocal.X, minCenterX, maxCenterX);
			if (!FMath::IsNearlyEqual(clampedX, curLocal.X, 0.1f))
			{
				const FVector newWorld = Axf.TransformPosition(FVector(clampedX, curLocal.Y, curLocal.Z));
				Plat->SetActorLocation(newWorld, /*bSweep=*/false);
			}
		}
		// -------------------------------------------------------------

		OutActors.Add(Plat);
	}
}

void ALaneLevelGenerator::DestroyAllRows()
{
	for (FRowBit& Row : LiveRows)
	{
		DespawnRow(Row);
	}
	LiveRows.Empty();
	NextRowIndex = 0;
	// park the cursor at current player Y so we can decide a fresh offset on resume
	CursorLocalY = PlayerLocalY();
}

void ALaneLevelGenerator::DestroyAllWalls(bool bDisableBlockers)
{
	// visual tiles
	for (auto* C : WallSpritesLeft)  if (C) C->DestroyComponent();
	for (auto* C : WallSpritesRight) if (C) C->DestroyComponent();
	WallSpritesLeft.Empty();
	WallSpritesRight.Empty();

	// reset streaming cursors
	WallTopY = 0.f;
	WallNextSpawnY = 0.f;

	// optionally disable the blocking boxes too
	if (bDisableBlockers)
	{
		if (LeftWall) { LeftWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);  LeftWall->SetHiddenInGame(true); }
		if (RightWall) { RightWall->SetCollisionEnabled(ECollisionEnabled::NoCollision); RightWall->SetHiddenInGame(true); }
	}
}

void ALaneLevelGenerator::PauseAndFlush(bool bAlsoWalls, bool bDisableWallBlockers)
{
	// stop generation
	bSpawnPlatforms = false;

	// clear spawned content
	DestroyAllRows();
	if (bAlsoWalls) DestroyAllWalls(bDisableWallBlockers);

	// reset “start after delay” state so resume is clean
	PlatformSpawnStartTime = 0.f;
	bFirstPlatformSpawn = true;
}

void ALaneLevelGenerator::ResumeSpawning(float StartDelaySeconds, float StartBelowPlayerScreens)
{
	// re-enable blockers if you had disabled them
	if (LeftWall) { LeftWall->SetHiddenInGame(false);  LeftWall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
	if (RightWall) { RightWall->SetHiddenInGame(false); RightWall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }

	// seed wall visuals again if you want them visible
	if (bShowWalls) InitWallsInfinite();

	// schedule spawning with your existing delay/first-spawn path
	PlatformSpawnDelay = StartDelaySeconds;
	PlatformSpawnStartTime = 0.f;     // let Tick set it on first frame
	bFirstPlatformSpawn = true;

	// choose where the very first row starts (below player)
	PlatformSpawnOffsetY = StartBelowPlayerScreens * ScreenWorldHeightUU;

	bSpawnPlatforms = true;
}