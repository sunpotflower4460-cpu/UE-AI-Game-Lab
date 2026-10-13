// In-engine automation tests. Run from the editor (Session Frontend > Automation) or:
//   UnrealEditor-Cmd LumiStack.uproject -ExecCmds="Automation RunTests LumiStack; Quit" -unattended -nullrhi -nosplash
#include "GameCore/LumiCore.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
bool AdvanceToAlignment(lumi::StackGame& Game)
{
	const float Dt = 1.0f / 240.0f;
	float Prev = Game.AlignmentError();
	for (int32 Step = 0; Step < 240 * 20; ++Step)
	{
		Game.Update(Dt);
		const float Cur = Game.AlignmentError();
		if ((Cur <= 0.0f) != (Prev <= 0.0f))
		{
			return true;
		}
		Prev = Cur;
	}
	return false;
}
}  // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLumiStackPerfectRunTest, "LumiStack.Core.PerfectRunReachesSummit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLumiStackPerfectRunTest::RunTest(const FString& Parameters)
{
	lumi::StackGame Game(7u);
	Game.StartRun();
	int32 StageClears = 0;
	for (int32 Guard = 0; Guard < 500 && Game.GetPhase() == lumi::Phase::Playing; ++Guard)
	{
		Game.ClearEvents();
		if (!AdvanceToAlignment(Game))
		{
			break;
		}
		Game.Tap();
		for (const lumi::Event& Event : Game.Events())
		{
			StageClears += Event.type == lumi::EventType::StageClear ? 1 : 0;
		}
	}
	TestTrue(TEXT("Run is won"), Game.GetPhase() == lumi::Phase::Won);
	TestEqual(TEXT("Summit floor"), Game.TopFloor(), lumi::TotalFloorsToWin());
	TestEqual(TEXT("Stage clears"), StageClears, lumi::StageCount());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLumiStackMissTest, "LumiStack.Core.MissEndsRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLumiStackMissTest::RunTest(const FString& Parameters)
{
	lumi::StackGame Game(3u);
	Game.StartRun();
	Game.Tap();  // the block starts fully off the tower
	TestTrue(TEXT("Game over after a miss"), Game.GetPhase() == lumi::Phase::GameOver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLumiStackSynthTest, "LumiStack.Core.SynthesizedAudio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLumiStackSynthTest::RunTest(const FString& Parameters)
{
	for (int32 Sfx = 0; Sfx < lumi::kSfxCount; ++Sfx)
	{
		const std::vector<float> Wave = lumi::SynthesizeSfx(static_cast<lumi::Sfx>(Sfx), 3, 44100);
		TestTrue(FString::Printf(TEXT("Sfx %d not empty"), Sfx), Wave.size() > 1000);
	}
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
