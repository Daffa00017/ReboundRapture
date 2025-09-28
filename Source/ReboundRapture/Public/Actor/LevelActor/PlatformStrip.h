// PlatFormStrip header

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PlatformStrip.generated.h"

UENUM(BlueprintType)
enum class ETileBand : uint8
{
	TwoCaps,   // [LeftCap][RightCap]
	Three,     // [LeftCap][Middle xN>=1][RightCap]
	Five       // [OuterLeft][MiddleLeft][Middle xN>=0][MiddleRight][OuterRight]
};

UENUM(BlueprintType)
enum class EPlatformKind : uint8 { Solid, Breakable, SpikeTop };

class UBoxComponent;
class UPaperSpriteComponent;

UCLASS()
class REBOUNDRAPTURE_API APlatformStrip : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APlatformStrip();

    UFUNCTION(BlueprintCallable) void BuildTiled(float TargetWidthUU);
    UFUNCTION(BlueprintCallable) void BuildTiledByCount(int32 TileCount);
    UFUNCTION(BlueprintCallable) void BuildTiledByCount_Safe(int32 TileCount);
    UFUNCTION(BlueprintCallable, Category = "Platform") void SetVisualKind(EPlatformKind InKind);
    UFUNCTION(BlueprintCallable, Category = "Platform|Render") void SetSpriteRollDegrees(float InRoll);
    UFUNCTION(BlueprintCallable, Category = "Collision|Debug") void SetCollisionVisible(bool bVisible);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiling") ETileBand Style = ETileBand::Three;

    // Assign these in the BP child (sprites from your artist)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiling") UPaperSprite* OuterLeft = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiling") UPaperSprite* MiddleLeft = nullptr;   // used in Five
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiling") UPaperSprite* Middle = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiling") UPaperSprite* MiddleRight = nullptr;   // used in Five
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tiling") UPaperSprite* OuterRight = nullptr;


    // If you want 1 row high, collision height = tileH; otherwise expose this
    UPROPERTY(EditAnywhere, Category = "Tiling") float CollisionHeightUU_Override = -1.f; // <0 = use tileH

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Render")
    float SpriteRollDeg = 0.f;

    // How much to shrink the collider on the left & right (UU each side)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float CollisionInsetXUU = 8.f;

    // Thickness along +Y (forward/back). If <=0, keep existing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float CollisionDepthUU = 24.f;

    // Grow/shrink the collision box (UU). All applied in the strip's local space.
// X = left/right across lanes, Y = fall axis, Z = out-of-plane (unchanged here).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float CollisionPadXUU = 0.f;            // +X/-X (symmetric)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float CollisionPadYUU = 0.f;            // front/back (symmetric)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float CollisionTopBoostYUU = 0.f;       // one-sided: lifts the TOP face upward (toward -Y)

    // If true, the collider's TOP face is placed at local Y=0 (sprite row), so it hugs the art.
    // If false, sizing is symmetric (top boost only raises the top by half and lowers center).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    bool bCollisionAnchorTopToRow = true;

    // Convenience if you want to push from the generator
    UFUNCTION(BlueprintCallable, Category = "Collision")
    void SetCollisionPads(float InPadX, float InPadY, float InTopBoostY);

    // Optional: let generator set them at spawn time
    UFUNCTION(BlueprintCallable, Category = "Collision")
    void SetCollisionDepthUU(float InDepth) { CollisionDepthUU = InDepth; }

    UFUNCTION(BlueprintCallable, Category = "Collision")
    void SetCollisionInsetXUU(float InInset) { CollisionInsetXUU = InInset; }

    UFUNCTION(BlueprintCallable, Category = "Collision")
    void SetCollisionAnchorTop(bool bAnchor) { bCollisionAnchorTopToRow = bAnchor; }

    UFUNCTION(BlueprintCallable, Category = "Collision")
    float GetCollisionHalfExtentX() const;

    // assign sprites (variants) for each kind
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    UPaperSprite* SolidMiddle = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    UPaperSprite* BreakableMiddle = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    UPaperSprite* SpikeTopMiddle = nullptr;

    // which set to use when building
    UFUNCTION(BlueprintCallable) void SetKind(EPlatformKind InKind) { VisualKind = InKind; }
    UFUNCTION(BlueprintCallable) EPlatformKind GetKind() const { return VisualKind; }

    // Misc
    UPROPERTY(EditAnywhere, Category = "Tiling") int32   TranslucentSortPriority = 10;
    UPROPERTY(EditAnywhere, Category = "Tiling") float   VisualYOffsetUU = 0.f;  // nudge if needed

    // Old API for strips; now it just calls BuildTiled
    UFUNCTION(BlueprintCallable) void SetSizeUU(float WidthUU, float HeightUU);

    UFUNCTION(BlueprintCallable, Category = "Platform")
    float GetCollisionHeightUU() const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void BuildDebugFallback(int32 TileCount);

protected:
    UPROPERTY(VisibleAnywhere) USceneComponent* Root;
    UPROPERTY(VisibleAnywhere) UBoxComponent* Box;
    // Keep one “marker” sprite; we’ll hide it and spawn children for tiles
    UPROPERTY(VisibleAnywhere) UPaperSpriteComponent* Sprite;

    // helpers
    FVector2f GetTileSizeUU(UPaperSprite* S) const; // (w,h) in UU from PPUU and source px
    void      ClearBuiltTiles();                    // delete previously spawned tile components
    void AddTile(UPaperSprite* S, float XLocal, float ZLocal);
    void ApplyCollisionSizing(float UsedWidthUU, float TileHeightUU);

    UPROPERTY(EditAnywhere, Category = "Visuals") EPlatformKind VisualKind = EPlatformKind::Solid;

    UPaperSprite* PickMiddle() const
    {
        switch (VisualKind)
        {
        default:
        case EPlatformKind::Solid:     return SolidMiddle ? SolidMiddle : Middle;
        case EPlatformKind::Breakable: return BreakableMiddle ? BreakableMiddle : Middle;
        case EPlatformKind::SpikeTop:  return SpikeTopMiddle ? SpikeTopMiddle : Middle;
        }
    }
};

