#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"

#include "LumiStackSaveGame.generated.h"

namespace LumiStackSave
{
inline const TCHAR* SlotName = TEXT("LumiStackProgress");
constexpr int32 UserIndex = 0;
}

/** Persistent player progress (best score etc.). Stored with UGameplayStatics save slots. */
UCLASS()
class LUMISTACK_API ULumiStackSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 BestScore = 0;

	UPROPERTY()
	int32 BestFloor = 0;

	UPROPERTY()
	int32 GamesPlayed = 0;

	UPROPERTY()
	int32 SummitsReached = 0;
};
