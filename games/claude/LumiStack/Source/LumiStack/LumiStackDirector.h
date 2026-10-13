#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GameCore/LumiCore.h"

#include "LumiStackDirector.generated.h"

class SLumiStackHUD;
class UAudioComponent;
class UCameraComponent;
class UDirectionalLightComponent;
class ULumiStackSaveGame;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;

/** Everything the Slate HUD needs to draw a frame. Owned by the director. */
struct FLumiHudState
{
	lumi::Phase Phase = lumi::Phase::Title;
	float PhaseAge = 0.0f;

	int32 Score = 0;
	int32 BestScore = 0;
	int32 Floor = 0;
	int32 Combo = 0;
	bool bNewBest = false;
	float ScorePulse = 0.0f;

	int32 StageNumber = 1;
	int32 StageCount = 1;
	FString StageName;
	float StageProgress = 0.0f;

	FString PopupText;
	float PopupAge = 100.0f;

	FString BannerTitle;
	FString BannerSubtitle;
	float BannerAge = 100.0f;

	FLinearColor Accent = FLinearColor::White;
};

USTRUCT()
struct FLumiTowerVisual
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	FVector TargetLocation = FVector::ZeroVector;
	FVector TargetScale = FVector::OneVector;
};

USTRUCT()
struct FLumiDebris
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	float Life = 0.0f;
	float ShrinkAfter = 0.0f;
	FVector BaseScale = FVector::OneVector;
};

USTRUCT()
struct FLumiRing
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material = nullptr;

	float Age = 0.0f;
	FVector2D BaseSize = FVector2D::UnitVector;
	FLinearColor Color = FLinearColor::White;
};

USTRUCT()
struct FLumiVoice
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Component = nullptr;

	double StopTime = 0.0;
};

/**
 * The whole game lives here: it owns the engine-independent lumi::StackGame simulation and
 * turns its events into Unreal visuals (meshes, lights, camera, physics debris), procedural
 * audio, haptics, save data and the Slate HUD. No content assets are required: only engine
 * built-in meshes/materials are referenced and everything else is generated at runtime.
 */
UCLASS()
class LUMISTACK_API ALumiStackDirector : public AActor
{
	GENERATED_BODY()

public:
	ALumiStackDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Single input action of the game (touch / click / space / gamepad). */
	void HandleTap();

	const FLumiHudState& GetHudState() const { return Hud; }

	// --- Camera -----------------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Camera")
	float CameraDistance = 2300.0f;

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Camera")
	float CameraPitch = 33.0f;

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Camera")
	float CameraYaw = 45.0f;

	/** Horizontal FOV used for portrait screens (phones). */
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Camera")
	float PortraitHorizontalFOV = 32.0f;

	/** Vertical FOV kept on landscape screens (editor, desktop). */
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Camera")
	float LandscapeVerticalFOV = 36.0f;

	/** How far below the tower top the camera aims (moves the tower up on screen). */
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Camera")
	float FocusBelowTop = 120.0f;

	// --- Lighting ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Lighting")
	float SunIntensityLux = 3.2f;

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Lighting")
	FRotator SunRotation = FRotator(-58.0f, 17.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Lighting")
	float FillIntensityCandela = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Lighting")
	float ExposureBias = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Lighting")
	float BloomIntensity = 0.35f;

	// --- Backdrop ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Backdrop")
	float BackdropDistance = 15000.0f;

	/** Multiplier applied to the unlit backdrop gradient (compensates exposure/tonemapping). */
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Backdrop")
	float BackdropBrightness = 1.0f;

	// --- Audio ------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Audio")
	float MasterVolume = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Audio")
	bool bAttractModeSounds = false;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Lumi Stack")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Lumi Stack")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Lumi Stack")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Lumi Stack")
	TObjectPtr<UPointLightComponent> Fill;

	UPROPERTY(VisibleAnywhere, Category = "Lumi Stack")
	TObjectPtr<UProceduralMeshComponent> Backdrop;

	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Assets")
	TObjectPtr<UStaticMesh> CubeMesh;

	/** Lit material with a "Color" vector parameter (engine BasicShapeMaterial). */
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Assets")
	TObjectPtr<UMaterialInterface> BlockMaterial;

	/** Unlit opaque material sampling texture parameter "SlateUI" (engine widget pass-through). */
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Assets")
	TObjectPtr<UMaterialInterface> UnlitOpaqueMaterial;

	/** Unlit translucent variant of the above, used for the "perfect" glow ring. */
	UPROPERTY(EditAnywhere, Category = "Lumi Stack|Assets")
	TObjectPtr<UMaterialInterface> UnlitTranslucentMaterial;

private:
	// Simulation -> visuals
	void ProcessGameEvents();
	void HandleEvent(const lumi::Event& Event);
	void RebuildTower();
	void ClearVisuals();

	UStaticMeshComponent* CreateBlockMesh(const lumi::Block& Block, bool bPedestal);
	void SetBlockTransform(UStaticMeshComponent* Mesh, const lumi::Block& Block, bool bPedestal) const;
	FVector BlockLocation(const lumi::Block& Block, bool bPedestal) const;
	FVector BlockScale(const lumi::Block& Block, bool bPedestal) const;
	float TopSurfaceZ() const;
	void SpawnDebris(const lumi::Block& Piece, int32 Axis, float Direction);
	void SpawnBurst(const lumi::Block& Around);
	void SpawnRing(const lumi::Block& Around, const FLinearColor& Color);

	// Per-frame updates
	void UpdateActiveBlock();
	void UpdateTowerAnimation(float DeltaSeconds);
	void UpdateDebris(float DeltaSeconds);
	void UpdateRings(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void UpdateBackdrop(float DeltaSeconds);
	void UpdateHud(float DeltaSeconds);
	void UpdateAutoPlay();
	void UpdateVoices();

	// Backdrop & textures
	void BuildBackdrop();
	void WriteBackdropTexture(const lumi::Rgb& Top, const lumi::Rgb& Bottom);
	UTexture2D* CreateRingTexture();
	void SetSkyTarget(int32 StageIndex, bool bInstant);

	// Audio / haptics / persistence
	void PlaySfx(lumi::Sfx Sfx, int32 Param, float Volume = 1.0f);
	void PlayHaptic(int32 Strength);
	void LoadProgress();
	void SaveProgress();
	void TryAttachToPlayer();

	TUniquePtr<lumi::StackGame> Game;
	TSharedPtr<SLumiStackHUD> HudWidget;
	FLumiHudState Hud;
	lumi::Phase LastPhase = lumi::Phase::Title;
	bool bAttachedToPlayer = false;

	UPROPERTY(Transient)
	TArray<FLumiTowerVisual> TowerVisuals;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ActiveMesh;

	UPROPERTY(Transient)
	TArray<FLumiDebris> Debris;

	UPROPERTY(Transient)
	TArray<FLumiRing> Rings;

	UPROPERTY(Transient)
	TArray<FLumiVoice> Voices;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BackdropTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RingTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackdropMaterial;

	UPROPERTY(Transient)
	TObjectPtr<ULumiStackSaveGame> Progress;

	TMap<int32, TArray<uint8>> SfxCache;

	// Camera state
	FVector CameraFocus = FVector::ZeroVector;
	float CurrentDistance = 0.0f;
	float ShakeTime = 0.0f;
	float ViewAspect = 9.0f / 19.5f;

	// Sky gradient transition
	lumi::Rgb SkyTopFrom;
	lumi::Rgb SkyBottomFrom;
	lumi::Rgb SkyTopTo;
	lumi::Rgb SkyBottomTo;
	float SkyBlend = 1.0f;
	bool bSkyDirty = true;

	// Debug autoplay
	float AutoPrevError = 0.0f;
	int32 AutoFloor = -1;
};
