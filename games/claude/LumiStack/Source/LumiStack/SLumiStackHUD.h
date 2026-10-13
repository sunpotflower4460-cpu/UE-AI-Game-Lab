#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ALumiStackDirector;
struct FLumiHudState;

/**
 * Code-only Slate HUD. It never receives input (hit-test invisible) and simply reads the
 * director's FLumiHudState every frame, so all animation timing lives in one place.
 */
class SLumiStackHUD : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLumiStackHUD) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ALumiStackDirector>, Director)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	const FLumiHudState& State() const;

	TSharedRef<SWidget> MakeLabel(const TAttribute<FText>& Text, int32 Size, FName Weight,
		const TAttribute<FSlateColor>& Color, int32 LetterSpacing = 0, bool bShadow = true) const;
	TSharedRef<SWidget> MakeFader(const TAttribute<FLinearColor>& Tint, const TSharedRef<SWidget>& Content) const;

	TSharedRef<SWidget> BuildPlayingLayer();
	TSharedRef<SWidget> BuildPopupLayer();
	TSharedRef<SWidget> BuildBannerLayer();
	TSharedRef<SWidget> BuildTitleLayer();
	TSharedRef<SWidget> BuildResultLayer();

	static float EaseOutBack(float T);
	float Pulse(float Speed) const;

	TWeakObjectPtr<ALumiStackDirector> Director;
	double StartTime = 0.0;

	FSlateNoResource NoBrush;
	FSlateRoundedBoxBrush TrackBrush{FLinearColor(1.0f, 1.0f, 1.0f, 0.28f), 4.0f};
	FSlateRoundedBoxBrush FillBrush{FLinearColor::White, 4.0f};
	FSlateRoundedBoxBrush CardBrush{FLinearColor(1.0f, 1.0f, 1.0f, 0.86f), 28.0f};
	FSlateRoundedBoxBrush PillBrush{FLinearColor(0.0f, 0.0f, 0.0f, 0.16f), 18.0f};
};
