// WebAssembly bridge for the browser preview. Exposes the exact same lumi::StackGame
// core that the Unreal module uses through a tiny C ABI.
#include "../Source/LumiStack/GameCore/LumiCore.h"

#include <memory>
#include <vector>

#define LUMI_EXPORT(name) extern "C" __attribute__((export_name(#name)))

namespace
{
std::unique_ptr<lumi::StackGame> g_game;
float g_buf[64];
constexpr int kEventStride = 18;
constexpr int kMaxEvents = 64;
float g_events[kEventStride * kMaxEvents];
std::vector<float> g_synth;

void WriteBlock(float* out, const lumi::Block& b)
{
	out[0] = b.cx;
	out[1] = b.cy;
	out[2] = b.sx;
	out[3] = b.sy;
	out[4] = static_cast<float>(b.floor);
	out[5] = static_cast<float>(b.stage);
}

lumi::StackGame& Game()
{
	if (!g_game)
	{
		g_game = std::make_unique<lumi::StackGame>();
	}
	return *g_game;
}
}  // namespace

LUMI_EXPORT(lumi_init) void lumi_init(unsigned seed) { g_game = std::make_unique<lumi::StackGame>(seed); }
LUMI_EXPORT(lumi_title) void lumi_title() { Game().EnterTitle(); }
LUMI_EXPORT(lumi_start) void lumi_start() { Game().StartRun(); }
LUMI_EXPORT(lumi_update) void lumi_update(float dt) { Game().Update(dt); }
LUMI_EXPORT(lumi_tap) int lumi_tap() { return Game().Tap() ? 1 : 0; }

LUMI_EXPORT(lumi_phase) int lumi_phase() { return static_cast<int>(Game().GetPhase()); }
LUMI_EXPORT(lumi_score) int lumi_score() { return Game().Score(); }
LUMI_EXPORT(lumi_stage) int lumi_stage() { return Game().StageIndex(); }
LUMI_EXPORT(lumi_combo) int lumi_combo() { return Game().Combo(); }
LUMI_EXPORT(lumi_top_floor) int lumi_top_floor() { return Game().TopFloor(); }
LUMI_EXPORT(lumi_stage_progress) float lumi_stage_progress() { return Game().StageProgress(); }
LUMI_EXPORT(lumi_stage_count) int lumi_stage_count() { return lumi::StageCount(); }
LUMI_EXPORT(lumi_total_floors) int lumi_total_floors() { return lumi::TotalFloorsToWin(); }
LUMI_EXPORT(lumi_block_height) float lumi_block_height() { return Game().GetTuning().blockHeight; }
LUMI_EXPORT(lumi_alignment_error) float lumi_alignment_error() { return Game().AlignmentError(); }
LUMI_EXPORT(lumi_has_active) int lumi_has_active() { return Game().HasActive() ? 1 : 0; }
LUMI_EXPORT(lumi_buf) float* lumi_buf() { return g_buf; }

LUMI_EXPORT(lumi_active) void lumi_active() { WriteBlock(g_buf, Game().Active()); }
LUMI_EXPORT(lumi_tower_count) int lumi_tower_count() { return static_cast<int>(Game().Tower().size()); }
LUMI_EXPORT(lumi_tower_block) void lumi_tower_block(int i)
{
	const auto& tower = Game().Tower();
	if (i >= 0 && static_cast<size_t>(i) < tower.size())
	{
		WriteBlock(g_buf, tower[static_cast<size_t>(i)]);
	}
}

LUMI_EXPORT(lumi_block_color) void lumi_block_color(int floor, int stage)
{
	lumi::Block b;
	b.floor = floor;
	b.stage = stage;
	const lumi::Rgb c = lumi::BlockColor(b);
	g_buf[0] = c.r;
	g_buf[1] = c.g;
	g_buf[2] = c.b;
}

LUMI_EXPORT(lumi_pedestal_color) void lumi_pedestal_color()
{
	const lumi::Rgb c = lumi::PedestalColor();
	g_buf[0] = c.r;
	g_buf[1] = c.g;
	g_buf[2] = c.b;
}

LUMI_EXPORT(lumi_stage_sky) void lumi_stage_sky(int stage)
{
	const lumi::StageInfo& s = lumi::GetStage(stage);
	g_buf[0] = s.skyTop.r;
	g_buf[1] = s.skyTop.g;
	g_buf[2] = s.skyTop.b;
	g_buf[3] = s.skyBottom.r;
	g_buf[4] = s.skyBottom.g;
	g_buf[5] = s.skyBottom.b;
	g_buf[6] = static_cast<float>(s.floors);
}

LUMI_EXPORT(lumi_stage_name) const char* lumi_stage_name(int stage) { return lumi::GetStage(stage).name; }

LUMI_EXPORT(lumi_event_stride) int lumi_event_stride() { return kEventStride; }
LUMI_EXPORT(lumi_events_ptr) float* lumi_events_ptr() { return g_events; }
// Copies pending events into the event buffer, clears them and returns the count.
LUMI_EXPORT(lumi_drain_events) int lumi_drain_events()
{
	const auto& events = Game().Events();
	int n = 0;
	for (const lumi::Event& e : events)
	{
		if (n >= kMaxEvents)
		{
			break;
		}
		float* o = g_events + n * kEventStride;
		o[0] = static_cast<float>(e.type);
		o[1] = static_cast<float>(e.axis);
		o[2] = static_cast<float>(e.combo);
		o[3] = static_cast<float>(e.stage);
		o[4] = static_cast<float>(e.scoreGain);
		o[5] = e.perfect ? 1.0f : 0.0f;
		WriteBlock(o + 6, e.block);
		WriteBlock(o + 12, e.piece);
		++n;
	}
	Game().ClearEvents();
	return n;
}

LUMI_EXPORT(lumi_synth) int lumi_synth(int sfx, int param, int sampleRate)
{
	g_synth = lumi::SynthesizeSfx(static_cast<lumi::Sfx>(sfx), param, sampleRate);
	return static_cast<int>(g_synth.size());
}
LUMI_EXPORT(lumi_synth_ptr) float* lumi_synth_ptr() { return g_synth.data(); }
