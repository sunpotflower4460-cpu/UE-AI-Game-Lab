#include "LumiStackGameMode.h"

#include "EngineUtils.h"
#include "LumiStack.h"
#include "LumiStackDirector.h"
#include "LumiStackPlayerController.h"

ALumiStackGameMode::ALumiStackGameMode()
{
	PlayerControllerClass = ALumiStackPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	DirectorClass = ALumiStackDirector::StaticClass();
}

void ALumiStackGameMode::StartPlay()
{
	UWorld* World = GetWorld();
	bool bHasDirector = false;
	for (TActorIterator<ALumiStackDirector> It(World); It; ++It)
	{
		bHasDirector = true;
		break;
	}
	if (!bHasDirector && DirectorClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ALumiStackDirector>(DirectorClass, FTransform::Identity, Params);
		UE_LOG(LogLumiStack, Log, TEXT("Spawned Lumi Stack director"));
	}

	Super::StartPlay();
}

void ALumiStackGameMode::RestartPlayer(AController* NewPlayer)
{
	// Intentionally empty: the game has no pawn. The director owns the camera.
}
