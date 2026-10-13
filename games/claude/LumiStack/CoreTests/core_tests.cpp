// Native unit tests for the engine-independent Lumi Stack core.
// Build & run: ./CoreTests/run_tests.sh
#include "../Source/LumiStack/GameCore/LumiCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

int g_failures = 0;
int g_checks = 0;

#define EXPECT(cond)                                                                 \
	do                                                                               \
	{                                                                                \
		++g_checks;                                                                  \
		if (!(cond))                                                                 \
		{                                                                            \
			++g_failures;                                                            \
			std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
		}                                                                            \
	} while (0)

#define EXPECT_NEAR(a, b, eps) EXPECT(std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= static_cast<double>(eps))

using namespace lumi;

const float kDt = 1.0f / 240.0f;

int CountEvents(const StackGame& g, EventType t)
{
	int n = 0;
	for (const Event& e : g.Events())
	{
		n += e.type == t ? 1 : 0;
	}
	return n;
}

const Event* FindEvent(const StackGame& g, EventType t)
{
	for (const Event& e : g.Events())
	{
		if (e.type == t)
		{
			return &e;
		}
	}
	return nullptr;
}

// Advances until the sliding block crosses `target` alignment error.
bool AdvanceToError(StackGame& g, float target, float maxSeconds = 20.0f)
{
	float prev = g.AlignmentError() - target;
	for (float t = 0.0f; t < maxSeconds; t += kDt)
	{
		g.Update(kDt);
		const float cur = g.AlignmentError() - target;
		if ((cur <= 0.0f) != (prev <= 0.0f) || std::fabs(cur) < 1e-4f)
		{
			return true;
		}
		prev = cur;
	}
	return false;
}

bool BlockIsSane(const Block& b)
{
	return std::isfinite(b.cx) && std::isfinite(b.cy) && b.sx > 0.0f && b.sy > 0.0f && b.sx <= 3.2001f && b.sy <= 3.2001f;
}

float Overlap1D(float c0, float s0, float c1, float s1)
{
	const float lo = std::fmax(c0 - s0 * 0.5f, c1 - s1 * 0.5f);
	const float hi = std::fmin(c0 + s0 * 0.5f, c1 + s1 * 0.5f);
	return hi - lo;
}

void TestStartState()
{
	std::printf("TestStartState\n");
	StackGame g(42);
	EXPECT(g.GetPhase() == Phase::Title);
	EXPECT(g.HasActive());
	g.ClearEvents();
	g.StartRun();
	EXPECT(g.GetPhase() == Phase::Playing);
	EXPECT(g.Tower().size() == 1u);
	EXPECT(g.Score() == 0);
	EXPECT(g.StageIndex() == 0);
	EXPECT(g.HasActive());
	EXPECT(g.Active().floor == 1);
	EXPECT(g.ActiveAxis() == kAxisX);
	EXPECT(CountEvents(g, EventType::Reset) == 1);
	EXPECT(CountEvents(g, EventType::Spawn) == 1);
	EXPECT_NEAR(std::fabs(g.AlignmentError()), g.GetTuning().travelRange, 1e-4f);
}

void TestPerfectDrop()
{
	std::printf("TestPerfectDrop\n");
	StackGame g(1);
	g.StartRun();
	g.ClearEvents();
	EXPECT(AdvanceToError(g, 0.0f));
	EXPECT(g.Tap());
	const Event* placed = FindEvent(g, EventType::Placed);
	EXPECT(placed != nullptr);
	if (placed)
	{
		EXPECT(placed->perfect);
		EXPECT(placed->combo == 1);
		EXPECT_NEAR(placed->block.sx, 3.2f, 1e-5f);
		EXPECT_NEAR(placed->block.cx, 0.0f, 1e-5f);
	}
	EXPECT(CountEvents(g, EventType::Sliced) == 0);
	EXPECT(g.Score() == 2);
	EXPECT(g.Tower().size() == 2u);
	EXPECT(g.ActiveAxis() == kAxisY);  // alternates
}

void TestSlice()
{
	std::printf("TestSlice\n");
	StackGame g(2);
	g.StartRun();
	g.ClearEvents();
	EXPECT(AdvanceToError(g, 1.0f));
	const float err = g.AlignmentError();
	EXPECT(g.Tap());
	const Event* placed = FindEvent(g, EventType::Placed);
	const Event* sliced = FindEvent(g, EventType::Sliced);
	EXPECT(placed != nullptr && sliced != nullptr);
	if (placed && sliced)
	{
		EXPECT(!placed->perfect);
		EXPECT_NEAR(placed->block.sx, 3.2f - std::fabs(err), 1e-4f);
		EXPECT_NEAR(sliced->piece.sx, std::fabs(err), 1e-4f);
		EXPECT_NEAR(placed->block.sx + sliced->piece.sx, 3.2f, 1e-4f);
		// The piece sits exactly adjacent to the placed block.
		EXPECT_NEAR(std::fabs(sliced->piece.cx - placed->block.cx), (placed->block.sx + sliced->piece.sx) * 0.5f, 1e-4f);
		// The placed block lies within the block below.
		EXPECT_NEAR(Overlap1D(placed->block.cx, placed->block.sx, 0.0f, 3.2f), placed->block.sx, 1e-4f);
		EXPECT(placed->combo == 0);
	}
	EXPECT(g.Score() == 1);
}

void TestMissEndsRun()
{
	std::printf("TestMissEndsRun\n");
	StackGame g(3);
	g.StartRun();
	g.ClearEvents();
	// Tap immediately: the block spawns travelRange away, which is beyond its size.
	EXPECT(g.Tap());
	EXPECT(CountEvents(g, EventType::Missed) == 1);
	EXPECT(CountEvents(g, EventType::GameOver) == 1);
	EXPECT(g.GetPhase() == Phase::GameOver);
	EXPECT(!g.HasActive());
	EXPECT(!g.Tap());
	g.Update(0.5f);
	EXPECT(g.GetPhase() == Phase::GameOver);
	g.StartRun();
	EXPECT(g.GetPhase() == Phase::Playing);
	EXPECT(g.Score() == 0);
}

void TestGrowAfterStreak()
{
	std::printf("TestGrowAfterStreak\n");
	StackGame g(4);
	g.StartRun();
	AdvanceToError(g, 1.2f);  // shrink X first
	g.Tap();
	const float shrunk = g.Top().sx;
	EXPECT(shrunk < 2.1f);
	int grewCount = 0;
	for (int i = 0; i < 6; ++i)
	{
		g.ClearEvents();
		AdvanceToError(g, 0.0f);
		g.Tap();
		grewCount += CountEvents(g, EventType::Grew);
	}
	EXPECT(g.Combo() == 6);
	EXPECT(grewCount == 2);  // combos 4 and 6 land on the shrunk X axis; Y is already full size
	EXPECT(g.Top().sx > shrunk + 0.2f);
	EXPECT(g.Top().sy <= 3.2f);
}

int PlayPerfect(StackGame& g, int& stageClears, int& wonEvents)
{
	stageClears = 0;
	wonEvents = 0;
	for (int guard = 0; guard < 1000 && g.GetPhase() == Phase::Playing; ++guard)
	{
		g.ClearEvents();
		if (!AdvanceToError(g, 0.0f))
		{
			break;
		}
		g.Tap();
		stageClears += CountEvents(g, EventType::StageClear);
		wonEvents += CountEvents(g, EventType::Won);
	}
	return g.TopFloor();
}

void TestStageClearAndWin()
{
	std::printf("TestStageClearAndWin\n");
	EXPECT(TotalFloorsToWin() == 100);
	EXPECT(StageCount() == 5);

	StackGame g(5);
	g.StartRun();
	const float speed0 = g.CurrentSpeed();
	// Damage the block a bit, then play perfectly to the first stage goal.
	AdvanceToError(g, 0.8f);
	g.Tap();
	while (g.StageIndex() == 0 && g.GetPhase() == Phase::Playing)
	{
		g.ClearEvents();
		AdvanceToError(g, 0.0f);
		g.Tap();
	}
	EXPECT(g.StageIndex() == 1);
	EXPECT(g.TopFloor() == GetStage(0).floors);
	const Event* clear = FindEvent(g, EventType::StageClear);
	EXPECT(clear != nullptr);
	if (clear)
	{
		EXPECT(clear->stage == 0);
		EXPECT_NEAR(clear->block.sx, 3.2f, 1e-5f);
		EXPECT_NEAR(clear->block.sy, 3.2f, 1e-5f);
	}
	EXPECT_NEAR(g.Top().sx, 3.2f, 1e-5f);
	EXPECT(g.CurrentSpeed() > speed0);
	EXPECT(g.CurrentTolerance() < g.GetTuning().perfectTolerance);
	EXPECT(g.Active().stage == 1);

	int clears = 0;
	int won = 0;
	const int top = PlayPerfect(g, clears, won);
	EXPECT(g.GetPhase() == Phase::Won);
	EXPECT(top == 100);
	EXPECT(clears == 4);
	EXPECT(won == 1);
	EXPECT(!g.HasActive());
	std::printf("  perfect run score: %d\n", g.Score());
}

void TestRandomFuzz()
{
	std::printf("TestRandomFuzz\n");
	uint32_t seed = 12345u;
	auto rnd = [&seed]() {
		seed = seed * 1103515245u + 12345u;
		return static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f;
	};
	int games = 0;
	int maxFloor = 0;
	long totalFloors = 0;
	bool allSane = true;
	for (int game = 0; game < 1500; ++game)
	{
		StackGame g(static_cast<uint32_t>(game + 1));
		g.StartRun();
		const float skill = 0.15f + 0.85f * rnd();
		for (int step = 0; step < 400 && g.GetPhase() == Phase::Playing; ++step)
		{
			const float target = (rnd() * 2.0f - 1.0f) * 2.2f * (1.0f - skill);
			AdvanceToError(g, target, 6.0f);
			g.Tap();
			for (const Event& e : g.Events())
			{
				if (e.type == EventType::Placed && !BlockIsSane(e.block))
				{
					allSane = false;
				}
				if (e.type == EventType::Sliced && !(e.piece.sx > 0.0f && e.piece.sy > 0.0f))
				{
					allSane = false;
				}
			}
			g.ClearEvents();
		}
		// Structural invariants on the final tower.
		const std::vector<Block>& t = g.Tower();
		for (size_t i = 1; i < t.size(); ++i)
		{
			if (t[i].floor != t[i - 1].floor + 1)
			{
				allSane = false;
			}
			if (!(Overlap1D(t[i].cx, t[i].sx, t[i - 1].cx, t[i - 1].sx) > 0.0f) ||
			    !(Overlap1D(t[i].cy, t[i].sy, t[i - 1].cy, t[i - 1].sy) > 0.0f))
			{
				allSane = false;
			}
		}
		EXPECT(g.GetPhase() == Phase::GameOver || g.GetPhase() == Phase::Won);
		maxFloor = maxFloor > g.TopFloor() ? maxFloor : g.TopFloor();
		totalFloors += g.TopFloor();
		++games;
	}
	EXPECT(allSane);
	std::printf("  %d games, avg floor %.1f, best floor %d\n", games, static_cast<double>(totalFloors) / games, maxFloor);
}

void TestAttractMode()
{
	std::printf("TestAttractMode\n");
	StackGame g(77);
	int resets = 0;
	int placed = 0;
	size_t maxTower = 0;
	for (int i = 0; i < 240 * 180; ++i)  // three simulated minutes
	{
		g.Update(kDt);
		resets += CountEvents(g, EventType::Reset);
		placed += CountEvents(g, EventType::Placed);
		EXPECT(CountEvents(g, EventType::GameOver) == 0);
		g.ClearEvents();
		maxTower = g.Tower().size() > maxTower ? g.Tower().size() : maxTower;
		if (g.GetPhase() != Phase::Title)
		{
			break;
		}
	}
	EXPECT(g.GetPhase() == Phase::Title);
	EXPECT(g.Score() == 0);
	EXPECT(placed > 40);
	EXPECT(resets >= 1);
	EXPECT(maxTower <= static_cast<size_t>(g.GetTuning().attractMaxFloors + 1));
	EXPECT(!g.Tap());  // taps are handled by the presentation layer in title
	std::printf("  placed %d blocks, %d resets, tallest %zu\n", placed, resets, maxTower);

	// Determinism: same seed, same sequence.
	StackGame a(9);
	StackGame b(9);
	for (int i = 0; i < 240 * 30; ++i)
	{
		a.Update(kDt);
		b.Update(kDt);
	}
	EXPECT(a.Tower().size() == b.Tower().size());
	EXPECT_NEAR(a.Top().cx, b.Top().cx, 0.0f);
}

void TestPalette()
{
	std::printf("TestPalette\n");
	for (int s = 0; s < StageCount(); ++s)
	{
		for (int f = 0; f < 40; ++f)
		{
			Block b;
			b.floor = f;
			b.stage = s;
			const Rgb c = BlockColor(b);
			EXPECT(c.r >= 0.0f && c.r <= 1.0f && c.g >= 0.0f && c.g <= 1.0f && c.b >= 0.0f && c.b <= 1.0f);
		}
	}
	const Rgb red = HsvToRgb(0.0f, 1.0f, 1.0f);
	EXPECT_NEAR(red.r, 1.0f, 1e-5f);
	EXPECT_NEAR(red.g, 0.0f, 1e-5f);
	const Rgb cyan = HsvToRgb(180.0f, 1.0f, 1.0f);
	EXPECT_NEAR(cyan.r, 0.0f, 1e-5f);
	EXPECT_NEAR(cyan.b, 1.0f, 1e-5f);
	EXPECT(GetStage(-3).name == GetStage(0).name);
	EXPECT(GetStage(99).name == GetStage(StageCount() - 1).name);
}

void TestSynth()
{
	std::printf("TestSynth\n");
	for (int s = 0; s < kSfxCount; ++s)
	{
		for (int param = 0; param < 20; param += 7)
		{
			const std::vector<float> wave = SynthesizeSfx(static_cast<Sfx>(s), param, 44100);
			EXPECT(wave.size() > 2000u && wave.size() < 44100u * 3u);
			float peak = 0.0f;
			bool finite = true;
			for (float v : wave)
			{
				finite = finite && std::isfinite(v);
				peak = std::fmax(peak, std::fabs(v));
			}
			EXPECT(finite);
			EXPECT(peak <= 1.0f);
			EXPECT(peak > 0.05f);  // audible
			EXPECT(std::fabs(wave.back()) < 0.01f);  // no click at the end
			const std::vector<int16_t> pcm = ToPcm16(wave);
			EXPECT(pcm.size() == wave.size());
		}
	}
}

}  // namespace

int main()
{
	TestStartState();
	TestPerfectDrop();
	TestSlice();
	TestMissEndsRun();
	TestGrowAfterStreak();
	TestStageClearAndWin();
	TestRandomFuzz();
	TestAttractMode();
	TestPalette();
	TestSynth();
	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
