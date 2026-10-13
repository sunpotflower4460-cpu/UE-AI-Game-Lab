#include "LumiStackDirector.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LumiStack.h"
#include "LumiStackSaveGame.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "RHI.h"
#include "SLumiStackHUD.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"

namespace
{

TAutoConsoleVariable<int32> CVarLumiAutoPlay(
	TEXT("lumi.AutoPlay"),
	0,
	TEXT("Lumi Stack debug autopilot. 0 = off, 1 = play the current run perfectly, 2 = also start/restart runs automatically."),
	ECVF_Default);

constexpr float UnitToCm = 100.0f;      // 1 core unit == 1 m
constexpr float PedestalDepth = 16.0f;  // core units below the pedestal top
constexpr int32 LumiSampleRate = 44100;
constexpr int32 BackdropTexWidth = 4;
constexpr int32 BackdropTexHeight = 256;
constexpr int32 RingTexSize = 64;

const FName ColorParam(TEXT("Color"));
const FName TextureParam(TEXT("SlateUI"));
const FName TintParam(TEXT("TintColorAndOpacity"));
const FName OpacityFromTextureParam(TEXT("OpacityFromTexture"));

FLinearColor ToLinear(const lumi::Rgb& C, float Alpha = 1.0f)
{
	FLinearColor Out = FLinearColor::FromSRGBColor(FColor(
		static_cast<uint8>(FMath::Clamp(C.r, 0.0f, 1.0f) * 255.0f + 0.5f),
		static_cast<uint8>(FMath::Clamp(C.g, 0.0f, 1.0f) * 255.0f + 0.5f),
		static_cast<uint8>(FMath::Clamp(C.b, 0.0f, 1.0f) * 255.0f + 0.5f)));
	Out.A = Alpha;
	return Out;
}

FColor ToColor(const lumi::Rgb& C)
{
	return FColor(
		static_cast<uint8>(FMath::Clamp(C.r, 0.0f, 1.0f) * 255.0f + 0.5f),
		static_cast<uint8>(FMath::Clamp(C.g, 0.0f, 1.0f) * 255.0f + 0.5f),
		static_cast<uint8>(FMath::Clamp(C.b, 0.0f, 1.0f) * 255.0f + 0.5f),
		255);
}

/** Builds a 100x100 quad. Both windings are emitted so it is visible from either side. */
void BuildQuad(UProceduralMeshComponent* Mesh, bool bVerticalFacingMinusX)
{
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	if (bVerticalFacingMinusX)
	{
		// Lies in the YZ plane; +Z is "up" on screen when attached in front of a camera.
		Vertices = {FVector(0, -50, 50), FVector(0, 50, 50), FVector(0, 50, -50), FVector(0, -50, -50)};
		Normals.Init(FVector(-1, 0, 0), 4);
	}
	else
	{
		Vertices = {FVector(-50, -50, 0), FVector(-50, 50, 0), FVector(50, 50, 0), FVector(50, -50, 0)};
		Normals.Init(FVector(0, 0, 1), 4);
	}
	const TArray<int32> Triangles = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};
	const TArray<FVector2D> UVs = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)};
	TArray<FLinearColor> Colors;
	Colors.Init(FLinearColor::White, 4);
	const TArray<FProcMeshTangent> Tangents;
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

}  // namespace

ALumiStackDirector::ALumiStackDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->bConstrainAspectRatio = false;
	Camera->SetFieldOfView(PortraitHorizontalFOV);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetCastShadows(true);
	Sun->DynamicShadowDistanceMovableLight = 6000.0f;

	Fill = CreateDefaultSubobject<UPointLightComponent>(TEXT("Fill"));
	Fill->SetupAttachment(Camera);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->IntensityUnits = ELightUnits::Candelas;
	Fill->SetCastShadows(false);
	Fill->SetRelativeLocation(FVector(0.0f, -350.0f, 300.0f));

	Backdrop = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Backdrop"));
	Backdrop->SetupAttachment(Camera);
	Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backdrop->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BlockMatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitFinder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentFinder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
	CubeMesh = CubeFinder.Object;
	BlockMaterial = BlockMatFinder.Object;
	UnlitOpaqueMaterial = UnlitFinder.Object;
	UnlitTranslucentMaterial = TranslucentFinder.Object;
}

void ALumiStackDirector::BeginPlay()
{
	Super::BeginPlay();

	Sun->SetWorldRotation(SunRotation);
	Sun->SetIntensity(SunIntensityLux);
	Sun->SetLightColor(FLinearColor(1.0f, 0.97f, 0.92f));
	Fill->SetIntensity(FillIntensityCandela);
	Fill->SetAttenuationRadius(6000.0f);
	Fill->SetLightColor(FLinearColor(0.92f, 0.95f, 1.0f));

	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = ExposureBias;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = 0;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = BloomIntensity;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.25f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.0f;
	Camera->PostProcessBlendWeight = 1.0f;

	if (!CubeMesh || !BlockMaterial)
	{
		UE_LOG(LogLumiStack, Error, TEXT("Engine basic shapes are missing; blocks cannot be drawn."));
	}
	if (!UnlitOpaqueMaterial)
	{
		UE_LOG(LogLumiStack, Warning, TEXT("Widget3DPassThrough_Opaque not found; backdrop falls back to a lit material."));
	}

	LoadProgress();

	const uint32 Seed = static_cast<uint32>(FPlatformTime::Cycles()) | 1u;
	Game = MakeUnique<lumi::StackGame>(Seed);

	BuildBackdrop();
	RingTexture = CreateRingTexture();
	SetSkyTarget(0, true);

	CurrentDistance = CameraDistance;
	CameraFocus = FVector(0.0f, 0.0f, TopSurfaceZ() - FocusBelowTop);

	ProcessGameEvents();
	TryAttachToPlayer();

	UE_LOG(LogLumiStack, Log, TEXT("Lumi Stack ready (%d stages, %d floors to the summit)"), lumi::StageCount(), lumi::TotalFloorsToWin());
}

void ALumiStackDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HudWidget.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
			}
		}
		HudWidget.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ALumiStackDirector::TryAttachToPlayer()
{
	if (bAttachedToPlayer)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!PC || !Viewport)
	{
		return;
	}

	PC->SetViewTarget(this);
	HudWidget = SNew(SLumiStackHUD).Director(this);
	Viewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), 10);
	bAttachedToPlayer = true;
}

// -------------------------------------------------------------------------------------
// Input
// -------------------------------------------------------------------------------------

void ALumiStackDirector::HandleTap()
{
	if (!Game)
	{
		return;
	}

	switch (Game->GetPhase())
	{
	case lumi::Phase::Title:
		Game->StartRun();
		PlaySfx(lumi::Sfx::Start, 0);
		break;
	case lumi::Phase::Playing:
		Game->Tap();
		break;
	case lumi::Phase::GameOver:
	case lumi::Phase::Won:
		if (Hud.PhaseAge > 0.7f)
		{
			Game->StartRun();
			PlaySfx(lumi::Sfx::Start, 0);
		}
		break;
	}
	ProcessGameEvents();
}

// -------------------------------------------------------------------------------------
// Tick
// -------------------------------------------------------------------------------------

void ALumiStackDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Game)
	{
		return;
	}

	TryAttachToPlayer();

	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	UpdateAutoPlay();
	Game->Update(Dt);
	ProcessGameEvents();

	UpdateActiveBlock();
	UpdateTowerAnimation(Dt);
	UpdateDebris(Dt);
	UpdateRings(Dt);
	UpdateCamera(Dt);
	UpdateBackdrop(Dt);
	UpdateHud(Dt);
	UpdateVoices();
}

void ALumiStackDirector::UpdateAutoPlay()
{
	const int32 Mode = CVarLumiAutoPlay.GetValueOnGameThread();
	if (Mode <= 0)
	{
		return;
	}

	const lumi::Phase Phase = Game->GetPhase();
	if (Phase == lumi::Phase::Playing && Game->HasActive())
	{
		const float Error = Game->AlignmentError();
		if (AutoFloor != Game->Active().floor)
		{
			AutoFloor = Game->Active().floor;
			AutoPrevError = Error;
			return;
		}
		// Tap on the frame closest to perfect alignment: either we just crossed zero, or
		// the next frame (extrapolated) would cross it and land further away than now.
		const float Next = Error + (Error - AutoPrevError);
		const bool bCrossed = (Error <= 0.0f) != (AutoPrevError <= 0.0f);
		const bool bWillCross = (Next <= 0.0f) != (Error <= 0.0f) && FMath::Abs(Error) <= FMath::Abs(Next);
		AutoPrevError = Error;
		if (bCrossed || bWillCross)
		{
			HandleTap();
		}
	}
	else if (Mode >= 2)
	{
		if (Phase == lumi::Phase::Title || Hud.PhaseAge > 2.0f)
		{
			HandleTap();
		}
	}
}

// -------------------------------------------------------------------------------------
// Simulation events -> visuals, audio and HUD
// -------------------------------------------------------------------------------------

void ALumiStackDirector::ProcessGameEvents()
{
	if (!Game)
	{
		return;
	}
	// Copy: handlers never mutate the game, but keep this robust anyway.
	const std::vector<lumi::Event> Events = Game->Events();
	Game->ClearEvents();
	for (const lumi::Event& Event : Events)
	{
		HandleEvent(Event);
	}
}

void ALumiStackDirector::HandleEvent(const lumi::Event& Event)
{
	const bool bRealRun = Game->GetPhase() != lumi::Phase::Title;
	const float SfxVolume = (bRealRun || bAttractModeSounds) ? 1.0f : 0.0f;

	switch (Event.type)
	{
	case lumi::EventType::Reset:
	{
		RebuildTower();
		SetSkyTarget(0, false);
		Hud.Score = 0;
		Hud.Combo = 0;
		Hud.bNewBest = false;
		Hud.PopupAge = 100.0f;
		Hud.BannerAge = 100.0f;
		if (bRealRun && Progress)
		{
			++Progress->GamesPlayed;
		}
		break;
	}
	case lumi::EventType::Spawn:
	{
		if (ActiveMesh)
		{
			ActiveMesh->DestroyComponent();
		}
		ActiveMesh = CreateBlockMesh(Event.block, false);
		if (ActiveMesh)
		{
			ActiveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		break;
	}
	case lumi::EventType::Placed:
	{
		UStaticMeshComponent* Mesh = ActiveMesh.Get();
		ActiveMesh = nullptr;
		if (!Mesh)
		{
			Mesh = CreateBlockMesh(Event.block, false);
		}
		if (Mesh)
		{
			Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			FLumiTowerVisual Visual;
			Visual.Mesh = Mesh;
			Visual.TargetLocation = BlockLocation(Event.block, false);
			Visual.TargetScale = BlockScale(Event.block, false);
			if (!Event.perfect)
			{
				// Sliced blocks snap to their cut shape instantly.
				Mesh->SetWorldLocation(Visual.TargetLocation);
				Mesh->SetWorldScale3D(Visual.TargetScale);
			}
			TowerVisuals.Add(Visual);
		}

		if (Event.perfect)
		{
			const lumi::Rgb Glow = lumi::BlockColor(Event.block);
			SpawnRing(Event.block, ToLinear(lumi::LerpRgb(Glow, lumi::Rgb{1.0f, 1.0f, 1.0f}, 0.6f)) * 2.5f);
			PlaySfx(lumi::Sfx::Perfect, Event.combo, 0.9f * SfxVolume);
			if (bRealRun)
			{
				Hud.PopupText = Event.combo >= 2 ? FString::Printf(TEXT("PERFECT  x%d"), Event.combo) : FString(TEXT("PERFECT"));
				Hud.PopupAge = 0.0f;
				PlayHaptic(1);
			}
		}
		else
		{
			PlaySfx(lumi::Sfx::Place, Event.block.floor, 0.8f * SfxVolume);
			if (bRealRun)
			{
				PlayHaptic(0);
			}
		}

		if (bRealRun)
		{
			Hud.Score = Game->Score();
			Hud.Combo = Event.combo;
			Hud.ScorePulse = 1.0f;
			UE_LOG(LogLumiStack, Log, TEXT("Placed floor %d (%s, combo %d) score %d"), Event.block.floor,
				Event.perfect ? TEXT("perfect") : TEXT("sliced"), Event.combo, Game->Score());
		}
		break;
	}
	case lumi::EventType::Sliced:
	{
		const float Dir = Event.axis == lumi::kAxisX ? (Event.piece.cx >= Game->Top().cx ? 1.0f : -1.0f)
		                                             : (Event.piece.cy >= Game->Top().cy ? 1.0f : -1.0f);
		SpawnDebris(Event.piece, Event.axis, Dir);
		break;
	}
	case lumi::EventType::Grew:
	{
		PlaySfx(lumi::Sfx::Grow, Event.combo, 0.7f * SfxVolume);
		break;
	}
	case lumi::EventType::StageClear:
	{
		if (TowerVisuals.Num() > 0)
		{
			FLumiTowerVisual& Top = TowerVisuals.Last();
			Top.TargetLocation = BlockLocation(Event.block, false);
			Top.TargetScale = BlockScale(Event.block, false);
		}
		SpawnBurst(Event.block);
		SpawnRing(Event.block, FLinearColor(3.0f, 3.0f, 3.0f, 1.0f));
		PlaySfx(lumi::Sfx::StageClear, Event.stage, SfxVolume);
		PlayHaptic(2);

		const bool bFinal = Event.stage + 1 >= lumi::StageCount();
		Hud.BannerTitle = bFinal ? FString(TEXT("SUMMIT!")) : FString(TEXT("STAGE CLEAR"));
		Hud.BannerSubtitle = bFinal ? FString(TEXT("THE TOWER TOUCHES THE STARS"))
		                            : FString::Printf(TEXT("NEXT  %s"), UTF8_TO_TCHAR(lumi::GetStage(Event.stage + 1).name));
		Hud.BannerAge = 0.0f;
		if (!bFinal)
		{
			SetSkyTarget(Event.stage + 1, false);
		}
		UE_LOG(LogLumiStack, Log, TEXT("Stage %d clear at floor %d"), Event.stage + 1, Event.block.floor);
		break;
	}
	case lumi::EventType::Missed:
	{
		if (ActiveMesh)
		{
			ActiveMesh->DestroyComponent();
			ActiveMesh = nullptr;
		}
		const float Dir = Event.axis == lumi::kAxisX ? (Event.piece.cx >= Game->Top().cx ? 1.0f : -1.0f)
		                                             : (Event.piece.cy >= Game->Top().cy ? 1.0f : -1.0f);
		SpawnDebris(Event.piece, Event.axis, Dir);
		PlaySfx(lumi::Sfx::Miss, 0, SfxVolume);
		if (bRealRun)
		{
			ShakeTime = 0.35f;
		}
		break;
	}
	case lumi::EventType::GameOver:
	{
		PlayHaptic(3);
		SaveProgress();
		UE_LOG(LogLumiStack, Log, TEXT("LUMI_RESULT phase=GameOver floor=%d score=%d best=%d"), Game->TopFloor(), Game->Score(), Hud.BestScore);
		break;
	}
	case lumi::EventType::Won:
	{
		if (Progress)
		{
			++Progress->SummitsReached;
		}
		SaveProgress();
		PlaySfx(lumi::Sfx::Win, 0, SfxVolume);
		UE_LOG(LogLumiStack, Log, TEXT("LUMI_RESULT phase=Won floor=%d score=%d best=%d"), Game->TopFloor(), Game->Score(), Hud.BestScore);
		break;
	}
	}
}

// -------------------------------------------------------------------------------------
// Blocks
// -------------------------------------------------------------------------------------

FVector ALumiStackDirector::BlockLocation(const lumi::Block& Block, bool bPedestal) const
{
	const float H = Game->GetTuning().blockHeight;
	float Z = (static_cast<float>(Block.floor) * H + H * 0.5f) * UnitToCm;
	if (bPedestal)
	{
		Z = (H - (H + PedestalDepth) * 0.5f) * UnitToCm;
	}
	return FVector(Block.cx * UnitToCm, Block.cy * UnitToCm, Z);
}

FVector ALumiStackDirector::BlockScale(const lumi::Block& Block, bool bPedestal) const
{
	// The engine cube is 100 units wide, so 1 core unit == scale 1.
	const float H = Game->GetTuning().blockHeight;
	return FVector(Block.sx, Block.sy, bPedestal ? H + PedestalDepth : H);
}

float ALumiStackDirector::TopSurfaceZ() const
{
	if (!Game)
	{
		return 0.0f;
	}
	return static_cast<float>(Game->TopFloor() + 1) * Game->GetTuning().blockHeight * UnitToCm;
}

void ALumiStackDirector::SetBlockTransform(UStaticMeshComponent* Mesh, const lumi::Block& Block, bool bPedestal) const
{
	Mesh->SetWorldLocationAndRotation(BlockLocation(Block, bPedestal), FRotator::ZeroRotator);
	Mesh->SetWorldScale3D(BlockScale(Block, bPedestal));
}

UStaticMeshComponent* ALumiStackDirector::CreateBlockMesh(const lumi::Block& Block, bool bPedestal)
{
	if (!CubeMesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(CubeMesh);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetCastShadow(true);
	Mesh->SetupAttachment(SceneRoot);
	Mesh->RegisterComponent();
	SetBlockTransform(Mesh, Block, bPedestal);

	if (BlockMaterial)
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BlockMaterial, Mesh);
		Mid->SetVectorParameterValue(ColorParam, ToLinear(bPedestal ? lumi::PedestalColor() : lumi::BlockColor(Block)));
		Mesh->SetMaterial(0, Mid);
	}
	return Mesh;
}

void ALumiStackDirector::ClearVisuals()
{
	for (FLumiTowerVisual& Visual : TowerVisuals)
	{
		if (Visual.Mesh)
		{
			Visual.Mesh->DestroyComponent();
		}
	}
	TowerVisuals.Reset();
	if (ActiveMesh)
	{
		ActiveMesh->DestroyComponent();
		ActiveMesh = nullptr;
	}
	for (FLumiDebris& Piece : Debris)
	{
		if (Piece.Mesh)
		{
			Piece.Mesh->DestroyComponent();
		}
	}
	Debris.Reset();
	for (FLumiRing& Ring : Rings)
	{
		if (Ring.Mesh)
		{
			Ring.Mesh->DestroyComponent();
		}
	}
	Rings.Reset();
}

void ALumiStackDirector::RebuildTower()
{
	ClearVisuals();
	const std::vector<lumi::Block>& Tower = Game->Tower();
	for (size_t Index = 0; Index < Tower.size(); ++Index)
	{
		const bool bPedestal = Index == 0;
		if (UStaticMeshComponent* Mesh = CreateBlockMesh(Tower[Index], bPedestal))
		{
			FLumiTowerVisual Visual;
			Visual.Mesh = Mesh;
			Visual.TargetLocation = BlockLocation(Tower[Index], bPedestal);
			Visual.TargetScale = BlockScale(Tower[Index], bPedestal);
			TowerVisuals.Add(Visual);
		}
	}
}

void ALumiStackDirector::SpawnDebris(const lumi::Block& Piece, int32 Axis, float Direction)
{
	UStaticMeshComponent* Mesh = CreateBlockMesh(Piece, false);
	if (!Mesh)
	{
		return;
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);

	const FVector Outward = Axis == lumi::kAxisX ? FVector(Direction, 0.0f, 0.0f) : FVector(0.0f, Direction, 0.0f);
	Mesh->SetPhysicsLinearVelocity(Outward * 120.0f + FVector(0.0f, 0.0f, 40.0f));
	const FVector Spin = Axis == lumi::kAxisX ? FVector(0.0f, Direction * 90.0f, 0.0f) : FVector(-Direction * 90.0f, 0.0f, 0.0f);
	Mesh->SetPhysicsAngularVelocityInDegrees(Spin);

	FLumiDebris Entry;
	Entry.Mesh = Mesh;
	Entry.Life = 0.0f;
	Entry.ShrinkAfter = 3.0f;
	Entry.BaseScale = Mesh->GetComponentScale();
	Debris.Add(Entry);
}

void ALumiStackDirector::SpawnBurst(const lumi::Block& Around)
{
	if (!CubeMesh)
	{
		return;
	}
	const FVector Centre = BlockLocation(Around, false) + FVector(0.0f, 0.0f, 60.0f);
	const int32 Count = 18;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		lumi::Block Spark = Around;
		Spark.floor = Around.floor + Index;
		UStaticMeshComponent* Mesh = CreateBlockMesh(Spark, false);
		if (!Mesh)
		{
			continue;
		}
		const float Angle = (static_cast<float>(Index) / static_cast<float>(Count)) * 2.0f * UE_PI;
		const float Size = FMath::FRandRange(0.12f, 0.22f);
		Mesh->SetWorldLocation(Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 60.0f);
		Mesh->SetWorldScale3D(FVector(Size));
		Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		Mesh->SetSimulatePhysics(true);
		const FVector Velocity(FMath::Cos(Angle) * FMath::FRandRange(250.0f, 520.0f), FMath::Sin(Angle) * FMath::FRandRange(250.0f, 520.0f), FMath::FRandRange(520.0f, 900.0f));
		Mesh->SetPhysicsLinearVelocity(Velocity);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-360.0f, 360.0f), FMath::FRandRange(-360.0f, 360.0f), 0.0f));

		FLumiDebris Entry;
		Entry.Mesh = Mesh;
		Entry.Life = 0.0f;
		Entry.ShrinkAfter = 1.4f;
		Entry.BaseScale = FVector(Size);
		Debris.Add(Entry);
	}
}

void ALumiStackDirector::SpawnRing(const lumi::Block& Around, const FLinearColor& Color)
{
	if (!UnlitTranslucentMaterial || !RingTexture)
	{
		return;
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetupAttachment(SceneRoot);
	Mesh->RegisterComponent();
	BuildQuad(Mesh, false);

	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(UnlitTranslucentMaterial, Mesh);
	Mid->SetTextureParameterValue(TextureParam, RingTexture);
	Mid->SetScalarParameterValue(OpacityFromTextureParam, 1.0f);
	Mid->SetVectorParameterValue(TintParam, Color);
	Mesh->SetMaterial(0, Mid);

	const float H = Game->GetTuning().blockHeight;
	Mesh->SetWorldLocation(FVector(Around.cx * UnitToCm, Around.cy * UnitToCm, static_cast<float>(Around.floor + 1) * H * UnitToCm + 1.5f));

	FLumiRing Ring;
	Ring.Mesh = Mesh;
	Ring.Material = Mid;
	Ring.Age = 0.0f;
	Ring.BaseSize = FVector2D(Around.sx, Around.sy);
	Ring.Color = Color;
	Rings.Add(Ring);
	UpdateRings(0.0f);
}

void ALumiStackDirector::UpdateActiveBlock()
{
	if (ActiveMesh && Game->HasActive())
	{
		SetBlockTransform(ActiveMesh, Game->Active(), false);
	}
}

void ALumiStackDirector::UpdateTowerAnimation(float DeltaSeconds)
{
	const float Alpha = 1.0f - FMath::Exp(-DeltaSeconds * 16.0f);
	for (FLumiTowerVisual& Visual : TowerVisuals)
	{
		if (!Visual.Mesh)
		{
			continue;
		}
		const FVector Location = Visual.Mesh->GetComponentLocation();
		const FVector Scale = Visual.Mesh->GetComponentScale();
		if (!Location.Equals(Visual.TargetLocation, 0.05f))
		{
			Visual.Mesh->SetWorldLocation(FMath::Lerp(Location, Visual.TargetLocation, Alpha));
		}
		if (!Scale.Equals(Visual.TargetScale, 0.0005f))
		{
			Visual.Mesh->SetWorldScale3D(FMath::Lerp(Scale, Visual.TargetScale, Alpha));
		}
	}
}

void ALumiStackDirector::UpdateDebris(float DeltaSeconds)
{
	const float KillZ = CameraFocus.Z - 4000.0f;
	for (int32 Index = Debris.Num() - 1; Index >= 0; --Index)
	{
		FLumiDebris& Piece = Debris[Index];
		Piece.Life += DeltaSeconds;
		bool bDead = !Piece.Mesh;
		if (!bDead)
		{
			const float ShrinkT = (Piece.Life - Piece.ShrinkAfter) / 0.6f;
			if (ShrinkT >= 1.0f || Piece.Mesh->GetComponentLocation().Z < KillZ)
			{
				bDead = true;
			}
			else if (ShrinkT > 0.0f)
			{
				Piece.Mesh->SetWorldScale3D(Piece.BaseScale * FMath::Max(0.01f, 1.0f - ShrinkT));
			}
		}
		if (bDead)
		{
			if (Piece.Mesh)
			{
				Piece.Mesh->DestroyComponent();
			}
			Debris.RemoveAtSwap(Index);
		}
	}
}

void ALumiStackDirector::UpdateRings(float DeltaSeconds)
{
	const float Duration = 0.55f;
	for (int32 Index = Rings.Num() - 1; Index >= 0; --Index)
	{
		FLumiRing& Ring = Rings[Index];
		Ring.Age += DeltaSeconds;
		const float T = FMath::Clamp(Ring.Age / Duration, 0.0f, 1.0f);
		if (T >= 1.0f || !Ring.Mesh)
		{
			if (Ring.Mesh)
			{
				Ring.Mesh->DestroyComponent();
			}
			Rings.RemoveAtSwap(Index);
			continue;
		}
		const float Ease = 1.0f - FMath::Pow(1.0f - T, 3.0f);
		const float Grow = 1.0f + 0.12f + 0.55f * Ease;
		// The ring texture's outline sits at ~90% of the quad, so oversize slightly.
		Ring.Mesh->SetWorldScale3D(FVector(Ring.BaseSize.X * Grow * 1.1f, Ring.BaseSize.Y * Grow * 1.1f, 1.0f));
		if (Ring.Material)
		{
			FLinearColor Tint = Ring.Color;
			Tint.A = 1.0f - T;
			Ring.Material->SetVectorParameterValue(TintParam, Tint);
		}
	}
}

// -------------------------------------------------------------------------------------
// Camera & backdrop
// -------------------------------------------------------------------------------------

void ALumiStackDirector::UpdateCamera(float DeltaSeconds)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			FVector2D Size;
			Viewport->GetViewportSize(Size);
			if (Size.X > 1.0 && Size.Y > 1.0)
			{
				ViewAspect = static_cast<float>(Size.X / Size.Y);
			}
		}
	}

	// Portrait keeps the horizontal FOV, landscape keeps the vertical FOV.
	const float LandscapeH = FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(LandscapeVerticalFOV * 0.5f)) * ViewAspect));
	const float HorizontalFOV = FMath::Clamp(FMath::Max(PortraitHorizontalFOV, LandscapeH), 10.0f, 120.0f);
	Camera->SetFieldOfView(HorizontalFOV);
	const float HalfH = FMath::DegreesToRadians(HorizontalFOV * 0.5f);
	const float HalfV = FMath::Atan(FMath::Tan(HalfH) / ViewAspect);

	const lumi::Block& Top = Game->Top();
	const float TopZ = TopSurfaceZ();
	FVector TargetFocus(Top.cx * UnitToCm, Top.cy * UnitToCm, TopZ - FocusBelowTop);
	float TargetDistance = CameraDistance;

	const lumi::Phase Phase = Game->GetPhase();
	if ((Phase == lumi::Phase::GameOver || Phase == lumi::Phase::Won) && Hud.PhaseAge > 0.4f)
	{
		// Pull back to admire the whole tower.
		const float TowerHeight = TopZ + 150.0f;
		const float Projected = TowerHeight * FMath::Cos(FMath::DegreesToRadians(CameraPitch));
		TargetFocus.Z = TopZ * 0.5f + 60.0f;
		TargetDistance = FMath::Max(CameraDistance, (Projected * 0.5f + 450.0f) / FMath::Tan(HalfV));
	}

	const float FocusAlpha = 1.0f - FMath::Exp(-DeltaSeconds * 4.0f);
	const float DistAlpha = 1.0f - FMath::Exp(-DeltaSeconds * 2.2f);
	CameraFocus = FMath::Lerp(CameraFocus, TargetFocus, FocusAlpha);
	CurrentDistance = FMath::Lerp(CurrentDistance, TargetDistance, DistAlpha);

	const FRotator ViewRotation(-CameraPitch, CameraYaw, 0.0f);
	FVector Location = CameraFocus - ViewRotation.Vector() * CurrentDistance;
	if (ShakeTime > 0.0f)
	{
		ShakeTime = FMath::Max(0.0f, ShakeTime - DeltaSeconds);
		const float Amp = 22.0f * (ShakeTime / 0.35f);
		Location += FVector(FMath::FRandRange(-Amp, Amp), FMath::FRandRange(-Amp, Amp), FMath::FRandRange(-Amp, Amp));
	}
	Camera->SetWorldLocationAndRotation(Location, ViewRotation);

	// Keep the backdrop covering the whole view at a fixed distance in front of the camera.
	const float Cover = 1.25f;
	const float HalfWidth = BackdropDistance * FMath::Tan(HalfH) * Cover;
	const float HalfHeight = BackdropDistance * FMath::Tan(HalfV) * Cover;
	Backdrop->SetRelativeLocation(FVector(BackdropDistance, 0.0f, 0.0f));
	Backdrop->SetRelativeScale3D(FVector(1.0f, HalfWidth * 2.0f / 100.0f, HalfHeight * 2.0f / 100.0f));
}

void ALumiStackDirector::BuildBackdrop()
{
	BuildQuad(Backdrop, true);

	if (UnlitOpaqueMaterial)
	{
		BackdropTexture = UTexture2D::CreateTransient(BackdropTexWidth, BackdropTexHeight, PF_B8G8R8A8, TEXT("LumiBackdrop"));
		if (BackdropTexture)
		{
			BackdropTexture->SRGB = true;
			BackdropTexture->Filter = TF_Bilinear;
			BackdropTexture->AddressX = TA_Clamp;
			BackdropTexture->AddressY = TA_Clamp;
			BackdropTexture->UpdateResource();
			BackdropMaterial = UMaterialInstanceDynamic::Create(UnlitOpaqueMaterial, this);
			BackdropMaterial->SetTextureParameterValue(TextureParam, BackdropTexture);
			BackdropMaterial->SetScalarParameterValue(OpacityFromTextureParam, 0.0f);
			BackdropMaterial->SetVectorParameterValue(TintParam, FLinearColor(BackdropBrightness, BackdropBrightness, BackdropBrightness, 1.0f));
		}
	}
	if (!BackdropMaterial && BlockMaterial)
	{
		// Fallback: a flat lit colour.
		BackdropMaterial = UMaterialInstanceDynamic::Create(BlockMaterial, this);
	}
	if (BackdropMaterial)
	{
		Backdrop->SetMaterial(0, BackdropMaterial);
	}
}

void ALumiStackDirector::SetSkyTarget(int32 StageIndex, bool bInstant)
{
	const lumi::StageInfo& Stage = lumi::GetStage(StageIndex);
	if (bInstant)
	{
		SkyTopFrom = Stage.skyTop;
		SkyBottomFrom = Stage.skyBottom;
		SkyBlend = 1.0f;
	}
	else
	{
		// Start from whatever is currently shown.
		const float Ease = SkyBlend * SkyBlend * (3.0f - 2.0f * SkyBlend);
		SkyTopFrom = lumi::LerpRgb(SkyTopFrom, SkyTopTo, Ease);
		SkyBottomFrom = lumi::LerpRgb(SkyBottomFrom, SkyBottomTo, Ease);
		SkyBlend = 0.0f;
	}
	SkyTopTo = Stage.skyTop;
	SkyBottomTo = Stage.skyBottom;
	bSkyDirty = true;
}

void ALumiStackDirector::UpdateBackdrop(float DeltaSeconds)
{
	if (SkyBlend < 1.0f)
	{
		SkyBlend = FMath::Min(1.0f, SkyBlend + DeltaSeconds / 1.4f);
		bSkyDirty = true;
	}
	if (!bSkyDirty)
	{
		return;
	}
	bSkyDirty = false;
	const float Ease = SkyBlend * SkyBlend * (3.0f - 2.0f * SkyBlend);
	const lumi::Rgb Top = lumi::LerpRgb(SkyTopFrom, SkyTopTo, Ease);
	const lumi::Rgb Bottom = lumi::LerpRgb(SkyBottomFrom, SkyBottomTo, Ease);

	if (BackdropTexture)
	{
		WriteBackdropTexture(Top, Bottom);
	}
	else if (BackdropMaterial)
	{
		BackdropMaterial->SetVectorParameterValue(ColorParam, ToLinear(lumi::LerpRgb(Top, Bottom, 0.5f)));
	}
}

void ALumiStackDirector::WriteBackdropTexture(const lumi::Rgb& Top, const lumi::Rgb& Bottom)
{
	const int32 PixelCount = BackdropTexWidth * BackdropTexHeight;
	uint8* Pixels = new uint8[PixelCount * 4];
	FColor* Out = reinterpret_cast<FColor*>(Pixels);
	for (int32 Y = 0; Y < BackdropTexHeight; ++Y)
	{
		const float T = static_cast<float>(Y) / static_cast<float>(BackdropTexHeight - 1);
		// Slight ease so the horizon band is wider than the top band.
		const float Shaped = FMath::Pow(T, 1.15f);
		const FColor Row = ToColor(lumi::LerpRgb(Top, Bottom, Shaped));
		for (int32 X = 0; X < BackdropTexWidth; ++X)
		{
			Out[Y * BackdropTexWidth + X] = Row;
		}
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, BackdropTexWidth, BackdropTexHeight);
	BackdropTexture->UpdateTextureRegions(0, 1, Region, BackdropTexWidth * 4, 4, Pixels,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}

UTexture2D* ALumiStackDirector::CreateRingTexture()
{
	UTexture2D* Texture = UTexture2D::CreateTransient(RingTexSize, RingTexSize, PF_B8G8R8A8, TEXT("LumiRing"));
	if (!Texture)
	{
		return nullptr;
	}
	Texture->SRGB = true;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->UpdateResource();

	uint8* Pixels = new uint8[RingTexSize * RingTexSize * 4];
	FColor* Out = reinterpret_cast<FColor*>(Pixels);
	for (int32 Y = 0; Y < RingTexSize; ++Y)
	{
		for (int32 X = 0; X < RingTexSize; ++X)
		{
			const float U = (static_cast<float>(X) + 0.5f) / RingTexSize * 2.0f - 1.0f;
			const float V = (static_cast<float>(Y) + 0.5f) / RingTexSize * 2.0f - 1.0f;
			const float D = FMath::Max(FMath::Abs(U), FMath::Abs(V));  // square distance 0..1
			const float Line = FMath::Exp(-FMath::Square((D - 0.9f) / 0.035f));
			const float Glow = FMath::Exp(-FMath::Square((D - 0.9f) / 0.12f)) * 0.45f;
			const float A = FMath::Clamp(Line + Glow, 0.0f, 1.0f) * (D > 0.995f ? 0.0f : 1.0f);
			Out[Y * RingTexSize + X] = FColor(255, 255, 255, static_cast<uint8>(A * 255.0f + 0.5f));
		}
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, RingTexSize, RingTexSize);
	Texture->UpdateTextureRegions(0, 1, Region, RingTexSize * 4, 4, Pixels,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
	return Texture;
}

// -------------------------------------------------------------------------------------
// HUD
// -------------------------------------------------------------------------------------

void ALumiStackDirector::UpdateHud(float DeltaSeconds)
{
	const lumi::Phase Phase = Game->GetPhase();
	if (Phase != LastPhase)
	{
		LastPhase = Phase;
		Hud.PhaseAge = 0.0f;
	}
	else
	{
		Hud.PhaseAge += DeltaSeconds;
	}
	Hud.Phase = Phase;
	Hud.PopupAge += DeltaSeconds;
	Hud.BannerAge += DeltaSeconds;
	Hud.ScorePulse = FMath::Max(0.0f, Hud.ScorePulse - DeltaSeconds * 4.0f);

	Hud.Floor = Game->TopFloor();
	Hud.StageNumber = Game->StageIndex() + 1;
	Hud.StageCount = lumi::StageCount();
	Hud.StageName = UTF8_TO_TCHAR(lumi::GetStage(Game->StageIndex()).name);
	Hud.StageProgress = Phase == lumi::Phase::Won ? 1.0f : Game->StageProgress();
	if (Phase != lumi::Phase::Title)
	{
		Hud.Score = Game->Score();
	}
	if (Progress)
	{
		Hud.BestScore = FMath::Max(Progress->BestScore, Hud.Score);
	}
	const lumi::Rgb Accent = lumi::BlockColor(Game->Top());
	Hud.Accent = ToLinear(Accent);
}

// -------------------------------------------------------------------------------------
// Audio, haptics, persistence
// -------------------------------------------------------------------------------------

void ALumiStackDirector::PlaySfx(lumi::Sfx Sfx, int32 Param, float Volume)
{
	if (Volume <= 0.0f || MasterVolume <= 0.0f)
	{
		return;
	}
	const int32 Key = static_cast<int32>(Sfx) * 1000 + FMath::Clamp(Param, 0, 999);
	TArray<uint8>* Cached = SfxCache.Find(Key);
	if (!Cached)
	{
		const std::vector<int16_t> Pcm = lumi::ToPcm16(lumi::SynthesizeSfx(Sfx, Param, LumiSampleRate));
		TArray<uint8>& Bytes = SfxCache.Add(Key);
		Bytes.SetNumUninitialized(static_cast<int32>(Pcm.size() * sizeof(int16_t)));
		FMemory::Memcpy(Bytes.GetData(), Pcm.data(), Bytes.Num());
		Cached = &Bytes;
	}

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(LumiSampleRate);
	Wave->NumChannels = 1;
	const float Seconds = static_cast<float>(Cached->Num()) / (2.0f * LumiSampleRate);
	Wave->Duration = Seconds;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(Cached->GetData(), Cached->Num());

	if (UAudioComponent* Component = UGameplayStatics::SpawnSound2D(this, Wave, Volume * MasterVolume))
	{
		FLumiVoice Voice;
		Voice.Component = Component;
		Voice.StopTime = FPlatformTime::Seconds() + Seconds + 0.1;
		Voices.Add(Voice);
	}
}

void ALumiStackDirector::UpdateVoices()
{
	// Procedural waves never report "finished" on their own, so stop them explicitly.
	const double Now = FPlatformTime::Seconds();
	for (int32 Index = Voices.Num() - 1; Index >= 0; --Index)
	{
		FLumiVoice& Voice = Voices[Index];
		if (!Voice.Component || Now >= Voice.StopTime)
		{
			if (Voice.Component)
			{
				Voice.Component->Stop();
			}
			Voices.RemoveAtSwap(Index);
		}
	}
}

void ALumiStackDirector::PlayHaptic(int32 Strength)
{
#if PLATFORM_IOS
	EMobileHapticsType Type = EMobileHapticsType::ImpactLight;
	switch (Strength)
	{
	case 0: Type = EMobileHapticsType::SelectionChanged; break;
	case 1: Type = EMobileHapticsType::ImpactLight; break;
	case 2: Type = EMobileHapticsType::FeedbackSuccess; break;
	default: Type = EMobileHapticsType::ImpactHeavy; break;
	}
	FPlatformMisc::PrepareMobileHaptics(Type);
	FPlatformMisc::TriggerMobileHaptics();
	FPlatformMisc::ReleaseMobileHaptics();
#endif
}

void ALumiStackDirector::LoadProgress()
{
	if (UGameplayStatics::DoesSaveGameExist(LumiStackSave::SlotName, LumiStackSave::UserIndex))
	{
		Progress = Cast<ULumiStackSaveGame>(UGameplayStatics::LoadGameFromSlot(LumiStackSave::SlotName, LumiStackSave::UserIndex));
	}
	if (!Progress)
	{
		Progress = Cast<ULumiStackSaveGame>(UGameplayStatics::CreateSaveGameObject(ULumiStackSaveGame::StaticClass()));
	}
	Hud.BestScore = Progress ? Progress->BestScore : 0;
}

void ALumiStackDirector::SaveProgress()
{
	if (!Progress || !Game)
	{
		return;
	}
	Hud.bNewBest = Game->Score() > Progress->BestScore;
	Progress->BestScore = FMath::Max(Progress->BestScore, Game->Score());
	Progress->BestFloor = FMath::Max(Progress->BestFloor, Game->TopFloor());
	Hud.BestScore = Progress->BestScore;
	UGameplayStatics::SaveGameToSlot(Progress, LumiStackSave::SlotName, LumiStackSave::UserIndex);
}
