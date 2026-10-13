#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "LumiStackPlayerController.generated.h"

class ALumiStackDirector;

/** Turns every kind of "tap" (touch, click, space, gamepad A) into a single game action. */
UCLASS()
class LUMISTACK_API ALumiStackPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALumiStackPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnTouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
	void OnTapPressed();
	void HandleTap();
	ALumiStackDirector* FindDirector();

	TWeakObjectPtr<ALumiStackDirector> CachedDirector;
	double LastTapTime = -1.0;
};
