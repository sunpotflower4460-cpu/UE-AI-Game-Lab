#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "LumiStackGameMode.generated.h"

class ALumiStackDirector;

/**
 * Game mode for Lumi Stack. There is no pawn: the whole game is driven by a single
 * ALumiStackDirector actor that the game mode spawns at start-up, so the game runs on
 * any (even empty) level.
 */
UCLASS()
class LUMISTACK_API ALumiStackGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALumiStackGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	UPROPERTY(EditDefaultsOnly, Category = "Lumi Stack")
	TSubclassOf<ALumiStackDirector> DirectorClass;
};
