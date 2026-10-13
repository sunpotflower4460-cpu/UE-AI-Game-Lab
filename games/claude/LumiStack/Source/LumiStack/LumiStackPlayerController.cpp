#include "LumiStackPlayerController.h"

#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "LumiStackDirector.h"

ALumiStackPlayerController::ALumiStackPlayerController()
{
	bShowMouseCursor = true;
	// There is no pawn; the director actor is the permanent view target.
	bAutoManageActiveCameraTarget = false;
	bEnableClickEvents = false;
	bEnableTouchEvents = false;
}

void ALumiStackPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);
}

void ALumiStackPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	check(InputComponent);

	InputComponent->BindTouch(IE_Pressed, this, &ALumiStackPlayerController::OnTouchPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALumiStackPlayerController::OnTapPressed);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ALumiStackPlayerController::OnTapPressed);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ALumiStackPlayerController::OnTapPressed);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ALumiStackPlayerController::OnTapPressed);
}

void ALumiStackPlayerController::OnTouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
	HandleTap();
}

void ALumiStackPlayerController::OnTapPressed()
{
	HandleTap();
}

void ALumiStackPlayerController::HandleTap()
{
	// "Use mouse for touch" can deliver both a touch and a click for one press.
	const double Now = FPlatformTime::Seconds();
	if (LastTapTime >= 0.0 && Now - LastTapTime < 0.06)
	{
		return;
	}
	LastTapTime = Now;

	if (ALumiStackDirector* Director = FindDirector())
	{
		Director->HandleTap();
	}
}

ALumiStackDirector* ALumiStackPlayerController::FindDirector()
{
	if (!CachedDirector.IsValid())
	{
		for (TActorIterator<ALumiStackDirector> It(GetWorld()); It; ++It)
		{
			CachedDirector = *It;
			break;
		}
	}
	return CachedDirector.Get();
}
