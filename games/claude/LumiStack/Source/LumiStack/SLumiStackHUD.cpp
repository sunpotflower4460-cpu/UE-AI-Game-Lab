#include "SLumiStackHUD.h"

#include "LumiStackDirector.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LumiStackHUD"

namespace
{

template <typename T, typename FuncType>
TAttribute<T> Attr(FuncType&& Func)
{
	return TAttribute<T>::CreateLambda(Forward<FuncType>(Func));
}

const FLinearColor White(1.0f, 1.0f, 1.0f, 1.0f);
const FLinearColor Ink(0.012f, 0.014f, 0.035f, 1.0f);        // very dark navy (linear)
const FLinearColor InkSoft(0.09f, 0.10f, 0.15f, 1.0f);
const FLinearColor Highlight = FLinearColor::FromSRGBColor(FColor(240, 96, 78));

EVisibility ShowIf(bool bCondition)
{
	return bCondition ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

TOptional<FSlateRenderTransform> ScaleTransform(float Scale)
{
	return TOptional<FSlateRenderTransform>(FSlateRenderTransform(Scale));
}

}  // namespace

void SLumiStackHUD::Construct(const FArguments& InArgs)
{
	Director = InArgs._Director;
	StartTime = FPlatformTime::Seconds();
	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
	[
		SNew(SSafeZone)
		.Padding(FMargin(20.0f, 16.0f))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[BuildTitleLayer()]
			+ SOverlay::Slot()[BuildPlayingLayer()]
			+ SOverlay::Slot()[BuildPopupLayer()]
			+ SOverlay::Slot()[BuildBannerLayer()]
			+ SOverlay::Slot()[BuildResultLayer()]
		]
	];
}

const FLumiHudState& SLumiStackHUD::State() const
{
	static const FLumiHudState Fallback;
	const ALumiStackDirector* Owner = Director.Get();
	return Owner ? Owner->GetHudState() : Fallback;
}

float SLumiStackHUD::EaseOutBack(float T)
{
	T = FMath::Clamp(T, 0.0f, 1.0f);
	const float C1 = 1.70158f;
	const float C3 = C1 + 1.0f;
	return 1.0f + C3 * FMath::Pow(T - 1.0f, 3.0f) + C1 * FMath::Pow(T - 1.0f, 2.0f);
}

float SLumiStackHUD::Pulse(float Speed) const
{
	const double Time = FPlatformTime::Seconds() - StartTime;
	return 0.55f + 0.45f * FMath::Sin(static_cast<float>(Time) * Speed);
}

TSharedRef<SWidget> SLumiStackHUD::MakeLabel(const TAttribute<FText>& Text, int32 Size, FName Weight,
	const TAttribute<FSlateColor>& Color, int32 LetterSpacing, bool bShadow) const
{
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Weight, Size);
	Font.LetterSpacing = LetterSpacing;
	return SNew(STextBlock)
		.Text(Text)
		.Font(Font)
		.ColorAndOpacity(Color)
		.Justification(ETextJustify::Center)
		.ShadowOffset(bShadow ? FVector2D(0.0f, 2.0f) : FVector2D::ZeroVector)
		.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.02f, bShadow ? 0.25f : 0.0f));
}

TSharedRef<SWidget> SLumiStackHUD::MakeFader(const TAttribute<FLinearColor>& Tint, const TSharedRef<SWidget>& Content) const
{
	return SNew(SBorder)
		.BorderImage(&NoBrush)
		.Padding(0.0f)
		.ColorAndOpacity(Tint)
		[
			Content
		];
}

// -------------------------------------------------------------------------------------
// In-game: score + stage progress
// -------------------------------------------------------------------------------------

TSharedRef<SWidget> SLumiStackHUD::BuildPlayingLayer()
{
	const float BarWidth = 180.0f;

	TSharedRef<SWidget> Score = MakeLabel(
		Attr<FText>([this]() { return FText::AsNumber(State().Score); }),
		80, TEXT("Bold"), White);
	Score->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Score->SetRenderTransform(Attr<TOptional<FSlateRenderTransform>>([this]()
	{
		return ScaleTransform(1.0f + 0.16f * State().ScorePulse);
	}));

	return SNew(SBox)
		.Visibility(Attr<EVisibility>([this]() { return ShowIf(State().Phase == lumi::Phase::Playing); }))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 24.0f, 0.0f, 0.0f)
			[
				Score
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(&PillBrush)
				.Padding(FMargin(18.0f, 8.0f, 18.0f, 10.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						MakeLabel(Attr<FText>([this]()
						{
							const FLumiHudState& S = State();
							return FText::Format(LOCTEXT("StageFmt", "STAGE {0}/{1}  \u00B7  {2}"),
								FText::AsNumber(S.StageNumber), FText::AsNumber(S.StageCount), FText::FromString(S.StageName));
						}), 12, TEXT("Bold"), White, 180, false)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 7.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(BarWidth)
						.HeightOverride(6.0f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SImage).Image(&TrackBrush)
							]
							+ SOverlay::Slot()
							.HAlign(HAlign_Left)
							[
								SNew(SBox)
								.WidthOverride(Attr<FOptionalSize>([this, BarWidth]()
								{
									return FOptionalSize(FMath::Max(6.0f, BarWidth * State().StageProgress));
								}))
								[
									SNew(SImage).Image(&FillBrush)
								]
							]
						]
					]
				]
			]
		];
}

// -------------------------------------------------------------------------------------
// "PERFECT x3" popup
// -------------------------------------------------------------------------------------

TSharedRef<SWidget> SLumiStackHUD::BuildPopupLayer()
{
	TSharedRef<SWidget> Popup = MakeLabel(
		Attr<FText>([this]() { return FText::FromString(State().PopupText); }),
		26, TEXT("Bold"), White, 260);
	Popup->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Popup->SetRenderTransform(Attr<TOptional<FSlateRenderTransform>>([this]()
	{
		return ScaleTransform(0.6f + 0.4f * EaseOutBack(State().PopupAge / 0.28f));
	}));

	return SNew(SVerticalBox)
		.Visibility(Attr<EVisibility>([this]()
		{
			return ShowIf(State().Phase == lumi::Phase::Playing && State().PopupAge < 0.95f);
		}))
		+ SVerticalBox::Slot().FillHeight(0.29f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			MakeFader(Attr<FLinearColor>([this]()
			{
				const float Age = State().PopupAge;
				const float In = FMath::Clamp(Age / 0.08f, 0.0f, 1.0f);
				const float Out = 1.0f - FMath::Clamp((Age - 0.6f) / 0.35f, 0.0f, 1.0f);
				return FLinearColor(1.0f, 1.0f, 1.0f, In * Out);
			}), Popup)
		]
		+ SVerticalBox::Slot().FillHeight(0.71f)[SNullWidget::NullWidget];
}

// -------------------------------------------------------------------------------------
// "STAGE CLEAR" banner
// -------------------------------------------------------------------------------------

TSharedRef<SWidget> SLumiStackHUD::BuildBannerLayer()
{
	TSharedRef<SWidget> Banner =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			MakeLabel(Attr<FText>([this]() { return FText::FromString(State().BannerTitle); }), 38, TEXT("Bold"), White, 120)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			MakeLabel(Attr<FText>([this]() { return FText::FromString(State().BannerSubtitle); }), 14, TEXT("Bold"), White, 320)
		];
	Banner->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Banner->SetRenderTransform(Attr<TOptional<FSlateRenderTransform>>([this]()
	{
		return ScaleTransform(0.7f + 0.3f * EaseOutBack(State().BannerAge / 0.35f));
	}));

	return SNew(SVerticalBox)
		.Visibility(Attr<EVisibility>([this]() { return ShowIf(State().BannerAge < 2.6f && State().Phase != lumi::Phase::Title); }))
		+ SVerticalBox::Slot().FillHeight(0.36f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			MakeFader(Attr<FLinearColor>([this]()
			{
				const float Age = State().BannerAge;
				const float In = FMath::Clamp(Age / 0.12f, 0.0f, 1.0f);
				const float Out = 1.0f - FMath::Clamp((Age - 2.0f) / 0.6f, 0.0f, 1.0f);
				return FLinearColor(1.0f, 1.0f, 1.0f, In * Out);
			}), Banner)
		]
		+ SVerticalBox::Slot().FillHeight(0.64f)[SNullWidget::NullWidget];
}

// -------------------------------------------------------------------------------------
// Title screen (over the attract-mode tower)
// -------------------------------------------------------------------------------------

TSharedRef<SWidget> SLumiStackHUD::BuildTitleLayer()
{
	TSharedRef<SWidget> Content =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(0.10f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			MakeLabel(LOCTEXT("TitleA", "LUMI"), 76, TEXT("Bold"), White, 420)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, -14.0f, 0.0f, 0.0f)
		[
			MakeLabel(LOCTEXT("TitleB", "STACK"), 76, TEXT("Bold"), White, 420)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 10.0f, 0.0f, 0.0f)
		[
			MakeLabel(LOCTEXT("Tagline", "BUILD A TOWER OF LIGHT"), 13, TEXT("Bold"), White, 420)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SBox)
			.Visibility(Attr<EVisibility>([this]() { return ShowIf(State().BestScore > 0); }))
			[
				MakeLabel(Attr<FText>([this]()
				{
					return FText::Format(LOCTEXT("BestFmt", "BEST  {0}"), FText::AsNumber(State().BestScore));
				}), 15, TEXT("Bold"), White, 300)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 40.0f)
		[
			MakeFader(Attr<FLinearColor>([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, Pulse(3.2f)); }),
				MakeLabel(LOCTEXT("TapToStart", "TAP TO START"), 18, TEXT("Bold"), White, 420))
		];

	return SNew(SBox)
		.Visibility(Attr<EVisibility>([this]() { return ShowIf(State().Phase == lumi::Phase::Title); }))
		[
			MakeFader(Attr<FLinearColor>([this]()
			{
				return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(State().PhaseAge / 0.5f, 0.0f, 1.0f));
			}), Content)
		];
}

// -------------------------------------------------------------------------------------
// Game over / summit card
// -------------------------------------------------------------------------------------

TSharedRef<SWidget> SLumiStackHUD::BuildResultLayer()
{
	auto IsWon = [this]() { return State().Phase == lumi::Phase::Won; };

	TSharedRef<SWidget> Card =
		SNew(SBox)
		.MinDesiredWidth(300.0f)
		[
			SNew(SBorder)
			.BorderImage(&CardBrush)
			.Padding(FMargin(40.0f, 26.0f, 40.0f, 28.0f))
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					MakeLabel(Attr<FText>([IsWon]()
					{
						return IsWon() ? LOCTEXT("Summit", "SUMMIT REACHED") : LOCTEXT("GameOver", "GAME OVER");
					}), 13, TEXT("Bold"), InkSoft, 320, false)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					MakeLabel(Attr<FText>([this]() { return FText::AsNumber(State().Score); }), 68, TEXT("Bold"), Ink, 0, false)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					MakeLabel(Attr<FText>([this]()
					{
						const FLumiHudState& S = State();
						return S.bNewBest ? LOCTEXT("NewBest", "NEW BEST!")
						                  : FText::Format(LOCTEXT("BestFmt2", "BEST  {0}"), FText::AsNumber(S.BestScore));
					}), 15, TEXT("Bold"), Attr<FSlateColor>([this]()
					{
						return FSlateColor(State().bNewBest ? Highlight : InkSoft);
					}), 260, false)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 12.0f, 0.0f, 0.0f)
				[
					MakeLabel(Attr<FText>([this]()
					{
						const FLumiHudState& S = State();
						return FText::Format(LOCTEXT("FloorFmt", "{0}F  \u00B7  STAGE {1}"), FText::AsNumber(S.Floor), FText::AsNumber(S.StageNumber));
					}), 11, TEXT("Regular"), InkSoft, 260, false)
				]
			]
		];
	Card->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Card->SetRenderTransform(Attr<TOptional<FSlateRenderTransform>>([this]()
	{
		return ScaleTransform(0.85f + 0.15f * EaseOutBack((State().PhaseAge - 0.45f) / 0.35f));
	}));

	return SNew(SVerticalBox)
		.Visibility(Attr<EVisibility>([this]()
		{
			const lumi::Phase Phase = State().Phase;
			return ShowIf((Phase == lumi::Phase::GameOver || Phase == lumi::Phase::Won) && State().PhaseAge > 0.45f);
		}))
		+ SVerticalBox::Slot().FillHeight(0.14f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			MakeFader(Attr<FLinearColor>([this]()
			{
				return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp((State().PhaseAge - 0.45f) / 0.25f, 0.0f, 1.0f));
			}), Card)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 40.0f)
		[
			MakeFader(Attr<FLinearColor>([this]()
			{
				const float Ready = FMath::Clamp((State().PhaseAge - 0.7f) / 0.3f, 0.0f, 1.0f);
				return FLinearColor(1.0f, 1.0f, 1.0f, Ready * Pulse(3.2f));
			}),
			MakeLabel(Attr<FText>([IsWon]()
			{
				return IsWon() ? LOCTEXT("PlayAgain", "TAP TO PLAY AGAIN") : LOCTEXT("Retry", "TAP TO RETRY");
			}), 18, TEXT("Bold"), White, 420))
		];
}

#undef LOCTEXT_NAMESPACE
