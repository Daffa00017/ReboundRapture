// PlatformStrip implementation


#include "Actor/LevelActor/PlatformStrip.h"
#include "Components/BoxComponent.h"

// Sets default values
APlatformStrip::APlatformStrip()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
    Box->SetupAttachment(Root);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Box->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

    Sprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
    Sprite->SetupAttachment(Root);
    Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sprite->SetHiddenInGame(true); // we build with child sprites
}


FVector2f APlatformStrip::GetTileSizeUU(UPaperSprite* S) const
{
    if (!S) return FVector2f(16.f, 16.f);
    const float ppuu = FMath::Max(S->GetPixelsPerUnrealUnit(), 0.001f);
    const FVector2D px = S->GetSourceSize();
    return FVector2f(px.X / ppuu, px.Y / ppuu);
}

void APlatformStrip::ClearBuiltTiles()
{
    TArray<USceneComponent*> children;
    Root->GetChildrenComponents(false, children);
    for (USceneComponent* c : children)
    {
        if (c != Box && c != Sprite) { c->DestroyComponent(); }
    }
}

void APlatformStrip::AddTile(UPaperSprite* S, float XLocal, float ZLocal)
{
    if (!S) return;
    auto* C = NewObject<UPaperSpriteComponent>(this);
    C->RegisterComponent();
    C->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
    C->SetSprite(S);

    // Put tiles in XZ plane so the camera (looking along ±Y) actually sees them.
    C->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));   // << KEY LINE
    C->SetRelativeLocation(FVector(XLocal, 0.f, ZLocal + VisualYOffsetUU));
    C->SetRelativeRotation(FRotator(0.f, 0.f, SpriteRollDeg));

    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetHiddenInGame(false);                          // make sure it’s visible
    C->SetMobility(EComponentMobility::Movable);
    C->SetTranslucentSortPriority(TranslucentSortPriority);

}

void APlatformStrip::ApplyCollisionSizing(float UsedWidthUU, float TileHeightUU)
{
    if (!Box) return;

    const float baseY = (CollisionHeightUU_Override > 0.f) ? CollisionHeightUU_Override : TileHeightUU;

    const float halfX = FMath::Max(UsedWidthUU * 0.5f + CollisionPadXUU, 2.f);

    // Symmetric thickening on Y plus optional "top boost" amount
    const float halfY = FMath::Max(baseY * 0.5f + CollisionPadYUU * 0.5f + CollisionTopBoostYUU * 0.5f, 2.f);

    // Keep Z as-is
    const float halfZ = FMath::Max(Box->GetUnscaledBoxExtent().Z, 2.f);

    // Center shift:
    //   - If anchoring top: put top face at local Y=0 => center at +halfY (remember +Y is down).
    //   - Else: symmetric, only apply half of the one-sided top boost upward (-Y).
    const float centerShiftY = bCollisionAnchorTopToRow ? (+halfY) : (-0.5f * CollisionTopBoostYUU);

    Box->SetBoxExtent(FVector(halfX, halfY, halfZ), /*bUpdateOverlaps=*/true);
    Box->SetRelativeLocation(FVector(0.f, centerShiftY, 0.f));
    Box->MarkRenderStateDirty();
}

void APlatformStrip::BuildTiled(float TargetWidthUU)
{
    // Pick the “baseline” tile (Middle if present, else any non-null)
    UPaperSprite* Baseline = Middle ? Middle : (OuterLeft ? OuterLeft : OuterRight);
    if (!Baseline) {
        UE_LOG(LogTemp, Error, TEXT("PlatformStrip: No sprite set (Middle/OuterLeft/OuterRight are null)."));
        return;
    }

    const FVector2f TileSize = GetTileSizeUU(Baseline);
    const float tileW = TileSize.X;
    const float tileH = TileSize.Y;

    // Minimum counts based on style
    int32 minTiles = 0;
    switch (Style)
    {
    case ETileBand::TwoCaps: minTiles = 2; break;
    case ETileBand::Three:   minTiles = 3; break;
    case ETileBand::Five:    minTiles = 5; break;
    }

    // Choose how many tiles we can fit (at least minTiles)
    const int32 tilesFit = FMath::Max(minTiles, FMath::FloorToInt(TargetWidthUU / tileW));

    // Compute used width and left edge so it’s CENTERED on the actor
    const float usedW = tilesFit * tileW;
    const float leftX = -usedW * 0.5f + tileW * 0.5f; // center of first tile

    ClearBuiltTiles();

    // Place tiles left->right according to style
    int32 i = 0;
    if (Style == ETileBand::TwoCaps)
    {
        AddTile(OuterLeft, leftX + i++ * tileW, 0.f);
        AddTile(OuterRight, leftX + i++ * tileW, 0.f);
    }
    else if (Style == ETileBand::Three)
    {
        AddTile(OuterLeft, leftX + i++ * tileW, 0.f);
        // middles (at least 1)
        for (; i < tilesFit - 1; ++i) AddTile(Middle, leftX + i * tileW, 0.f);
        AddTile(OuterRight, leftX + i * tileW, 0.f);
    }
    else // Five
    {
        AddTile(OuterLeft, leftX + i++ * tileW, 0.f);
        AddTile(MiddleLeft, leftX + i++ * tileW, 0.f);
        // core middles (may be 0 if tilesFit==5)
        for (; i < tilesFit - 2; ++i) AddTile(Middle, leftX + i * tileW, 0.f);
        AddTile(MiddleRight, leftX + i++ * tileW, 0.f);
        AddTile(OuterRight, leftX + i * tileW, 0.f);
    }

    const float collH = (CollisionHeightUU_Override > 0.f) ? CollisionHeightUU_Override : tileH;
    // X = width, Y = (thin) depth, Z = height
    Box->SetBoxExtent(FVector(usedW * 0.5f, CollisionDepthUU * 0.5f, collH * 0.5f));
    Box->SetRelativeLocation(FVector(0.f, 0.f, 0.f)); // keep centered; we top-align in the generator

}

void APlatformStrip::BuildTiledByCount(int32 TileCount)
{
    // Pick a baseline sprite to read size/PPUU (prefer hazard-aware middle)
    UPaperSprite* Baseline = PickMiddle();
    if (!Baseline) Baseline = Middle ? Middle : (OuterLeft ? OuterLeft : OuterRight);
    if (!Baseline) return;

    const FVector2f Tile = GetTileSizeUU(Baseline);
    float tileW = Tile.X;
    const float tileH = Tile.Y;

    if (TileCount <= 0) TileCount = 1;

    ClearBuiltTiles();

    // --- Style-specific placement ---
    if (Style == ETileBand::TwoCaps)
    {
        // Use actual cap widths so collider span matches visuals
        const float wL = OuterLeft ? GetTileSizeUU(OuterLeft).X : tileW;
        const float wR = OuterRight ? GetTileSizeUU(OuterRight).X : tileW;
        const float usedW = wL + wR;

        // Center whole band; place each cap at its own half-width
        const float leftEdge = -0.5f * usedW;
        const float xL = leftEdge + 0.5f * wL;
        const float xR = leftEdge + wL + 0.5f * wR;

        AddTile(OuterLeft, xL, 0.f);
        AddTile(OuterRight, xR, 0.f);

        ApplyCollisionSizing(/*UsedWidthUU=*/usedW, /*TileHeightUU=*/tileH);
        return;
    }

    // For 3/5 styles, we assume uniform tile width (taken from Baseline)
    int32 tilesUsed = TileCount;
    if (Style == ETileBand::Three) tilesUsed = FMath::Max(TileCount, 3);
    else                           tilesUsed = FMath::Max(TileCount, 5);

    const float usedW = tilesUsed * tileW;
    const float leftX = -0.5f * usedW + 0.5f * tileW;

    if (Style == ETileBand::Three)
    {
        int32 i = 0;
        AddTile(OuterLeft, leftX + (i++) * tileW, 0.f);

        // Fill with hazard-aware middle if available
        UPaperSprite* MidFill = PickMiddle();
        for (; i < tilesUsed - 1; ++i)
        {
            AddTile(MidFill ? MidFill : Middle, leftX + i * tileW, 0.f);
        }

        AddTile(OuterRight, leftX + (i)*tileW, 0.f);
    }
    else // Five
    {
        int32 i = 0;
        AddTile(OuterLeft, leftX + (i++) * tileW, 0.f);
        AddTile(MiddleLeft, leftX + (i++) * tileW, 0.f);

        UPaperSprite* MidFill = PickMiddle();
        for (; i < tilesUsed - 2; ++i)
        {
            AddTile(MidFill ? MidFill : Middle, leftX + i * tileW, 0.f);
        }

        AddTile(MiddleRight, leftX + (i++) * tileW, 0.f);
        AddTile(OuterRight, leftX + (i)*tileW, 0.f);
    }

    // Size the real collider from the built span (pads/top-anchoring applied inside)
    ApplyCollisionSizing(/*UsedWidthUU=*/usedW, /*TileHeightUU=*/tileH);
}


void APlatformStrip::SetVisualKind(EPlatformKind InKind)
{
    VisualKind = InKind;
}

void APlatformStrip::SetSpriteRollDegrees(float InRoll)
{
    SpriteRollDeg = InRoll;
}

void APlatformStrip::SetCollisionVisible(bool bVisible)
{
    if (!Box) return;

    // Make the shape component render its wireframe even when not selected
    Box->bDrawOnlyIfSelected = false;

    // Show it in game
    Box->SetHiddenInGame(!bVisible);
    Box->SetVisibility(bVisible, true);

    // Make sure render state updates
    Box->MarkRenderStateDirty();
}

void APlatformStrip::BuildTiledByCount_Safe(int32 TileCount)
{
    // Validate sprites needed for the chosen Style.
    // We favor the middle via PickMiddle() so hazard kinds can swap visuals.
    auto WarnFallback = [&](const TCHAR* Reason)
        {
            UE_LOG(LogTemp, Warning, TEXT("PlatformStrip: missing sprite(s) for Style %d (%s). Fallback to collider-only."),
                int32(Style), Reason);
            BuildDebugFallback(TileCount);
        };

    switch (Style)
    {
    case ETileBand::TwoCaps:
        if (!(OuterLeft && OuterRight)) { WarnFallback(TEXT("TwoCaps requires OuterLeft/OuterRight")); return; }
        break;

    case ETileBand::Three:
        if (!(OuterLeft && PickMiddle() && OuterRight)) { WarnFallback(TEXT("Three requires OuterLeft/Middle(or kind)/OuterRight")); return; }
        break;

    case ETileBand::Five:
        if (!(OuterLeft && MiddleLeft && PickMiddle() && MiddleRight && OuterRight))
        {
            WarnFallback(TEXT("Five requires OuterLeft/MiddleLeft/Middle(or kind)/MiddleRight/OuterRight"));
            return;
        }
        break;
    }

    // If we’re here, the sprite set is sufficient. Delegate to the regular tiler.
    // Let the tiler compute collision height unless explicitly overridden.
    if (CollisionHeightUU_Override <= 0.f) CollisionHeightUU_Override = -1.f;
    BuildTiledByCount(TileCount);
}

void APlatformStrip::SetCollisionPads(float InPadX, float InPadY, float InTopBoostY)
{
    CollisionPadXUU = InPadX;
    CollisionPadYUU = InPadY;
    CollisionTopBoostYUU = InTopBoostY;
}

float APlatformStrip::GetCollisionHalfExtentX() const
{
    return (Box ? Box->GetScaledBoxExtent().X : 0.f);
}

void APlatformStrip::SetSizeUU(float WidthUU, float HeightUU)
{
    // Forward to tiled with width; keep height via CollisionHeightUU_Override if you like
    BuildTiled(WidthUU);
}

float APlatformStrip::GetCollisionHeightUU() const
{
    // Box extent.Y is half-height in UU (local). ScaledBoxExtent covers component scale.
    return Box ? (Box->GetScaledBoxExtent().Z * 2.f) : 0.f;
}

void APlatformStrip::BuildDebugFallback(int32 TileCount)
{
    // Minimal visual or none; we only care about the collider span here
    ClearBuiltTiles();

    // Derive a reasonable tile size from any available sprite; if none, use a sane default
    float tileW = 32.f, tileH = 16.f;
    if (UPaperSprite* Ref = Middle ? Middle : (OuterLeft ? OuterLeft : OuterRight))
    {
        const FVector2f Tile = GetTileSizeUU(Ref);
        tileW = Tile.X; tileH = Tile.Y;
    }

    if (TileCount <= 0) TileCount = 1;
    const float usedW = TileCount * tileW;

    // Just size the collider; no sprite tiles needed for the fallback
    ApplyCollisionSizing(/*UsedWidthUU=*/usedW, /*TileHeightUU=*/tileH);
}

