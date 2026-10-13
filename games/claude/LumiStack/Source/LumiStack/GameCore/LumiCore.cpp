// Lumi Stack - engine-independent game core. See LumiCore.h.
#include "LumiCore.h"

#include <cmath>

namespace lumi
{

namespace
{

constexpr float kLumiPi = 3.14159265358979f;

const StageInfo kStages[] = {
	// name       floors  sky top                       sky bottom                    hue    step  sat    val
	{"DAWN",      12, {0.99f, 0.84f, 0.71f}, {0.56f, 0.64f, 0.88f}, 335.0f, 3.5f, 0.52f, 0.96f},
	{"LAGOON",    16, {0.67f, 0.95f, 0.88f}, {0.16f, 0.42f, 0.62f}, 178.0f, 3.0f, 0.46f, 0.90f},
	{"EMBER",     20, {1.00f, 0.66f, 0.50f}, {0.36f, 0.22f, 0.55f},  38.0f, 2.5f, 0.62f, 0.98f},
	{"AURORA",    24, {0.45f, 0.88f, 0.72f}, {0.10f, 0.14f, 0.36f}, 255.0f, 3.0f, 0.50f, 0.95f},
	{"STARLIGHT", 28, {0.25f, 0.18f, 0.45f}, {0.03f, 0.05f, 0.16f}, 190.0f, 4.0f, 0.45f, 1.00f},
};

constexpr int kStageCount = static_cast<int>(sizeof(kStages) / sizeof(kStages[0]));

float Clamp01(float v)
{
	return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

float Sign(float v)
{
	return v < 0.0f ? -1.0f : 1.0f;
}

}  // namespace

int StageCount()
{
	return kStageCount;
}

const StageInfo& GetStage(int stageIndex)
{
	if (stageIndex < 0)
	{
		stageIndex = 0;
	}
	if (stageIndex >= kStageCount)
	{
		stageIndex = kStageCount - 1;
	}
	return kStages[stageIndex];
}

int TotalFloorsToWin()
{
	int total = 0;
	for (const StageInfo& s : kStages)
	{
		total += s.floors;
	}
	return total;
}

Rgb HsvToRgb(float hueDegrees, float saturation, float value)
{
	float h = std::fmod(hueDegrees, 360.0f);
	if (h < 0.0f)
	{
		h += 360.0f;
	}
	const float c = value * saturation;
	const float hp = h / 60.0f;
	const float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	if (hp < 1.0f)      { r = c; g = x; }
	else if (hp < 2.0f) { r = x; g = c; }
	else if (hp < 3.0f) { g = c; b = x; }
	else if (hp < 4.0f) { g = x; b = c; }
	else if (hp < 5.0f) { r = x; b = c; }
	else                { r = c; b = x; }
	const float m = value - c;
	return Rgb{Clamp01(r + m), Clamp01(g + m), Clamp01(b + m)};
}

Rgb LerpRgb(const Rgb& a, const Rgb& b, float t)
{
	t = Clamp01(t);
	return Rgb{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Rgb ScaleRgb(const Rgb& c, float s)
{
	return Rgb{Clamp01(c.r * s), Clamp01(c.g * s), Clamp01(c.b * s)};
}

Rgb BlockColor(const Block& block)
{
	const StageInfo& info = GetStage(block.stage);
	int stageStart = 0;
	for (int i = 0; i < block.stage && i < kStageCount; ++i)
	{
		stageStart += kStages[i].floors;
	}
	const int local = block.floor - stageStart;
	return HsvToRgb(info.hueStart + info.hueStep * static_cast<float>(local), info.saturation, info.value);
}

Rgb PedestalColor()
{
	return Rgb{0.93f, 0.92f, 0.95f};
}

// ---------------------------------------------------------------------------
// StackGame
// ---------------------------------------------------------------------------

StackGame::StackGame(uint32_t seed, const Tuning& tuning)
	: tuning_(tuning)
	, rng_(seed != 0u ? seed : 0x2545F491u)
{
	EnterTitle();
}

float StackGame::NextRandom01()
{
	// xorshift32
	uint32_t x = rng_;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rng_ = x;
	return static_cast<float>(x >> 8) / 16777216.0f;
}

void StackGame::ResetTower()
{
	tower_.clear();
	Block base;
	base.sx = tuning_.baseSize;
	base.sy = tuning_.baseSize;
	tower_.push_back(base);
	hasActive_ = false;
	score_ = 0;
	stage_ = 0;
	combo_ = 0;
	perfects_ = 0;
	attractResetTimer_ = -1.0f;

	Event e;
	e.type = EventType::Reset;
	e.block = base;
	Push(e);
}

void StackGame::EnterTitle()
{
	phase_ = Phase::Title;
	ResetTower();
	SpawnActive();
}

void StackGame::StartRun()
{
	phase_ = Phase::Playing;
	ResetTower();
	SpawnActive();
}

int StackGame::StageStartFloor() const
{
	int start = 0;
	for (int i = 0; i < stage_ && i < kStageCount; ++i)
	{
		start += kStages[i].floors;
	}
	return start;
}

int StackGame::StageGoalFloor() const
{
	return StageStartFloor() + GetStage(stage_).floors;
}

int StackGame::StageOfFloor(int floor) const
{
	int goal = 0;
	for (int i = 0; i < kStageCount; ++i)
	{
		goal += kStages[i].floors;
		if (floor <= goal)
		{
			return i;
		}
	}
	return kStageCount - 1;
}

float StackGame::StageProgress() const
{
	const int start = StageStartFloor();
	const int floors = GetStage(stage_).floors;
	return Clamp01(static_cast<float>(TopFloor() - start) / static_cast<float>(floors));
}

float StackGame::CurrentSpeed() const
{
	const int intoStage = TopFloor() - StageStartFloor();
	return tuning_.baseSpeed + tuning_.speedPerStage * static_cast<float>(stage_) +
	       tuning_.speedPerFloor * static_cast<float>(intoStage < 0 ? 0 : intoStage);
}

float StackGame::CurrentTolerance() const
{
	const float t = tuning_.perfectTolerance - tuning_.tolerancePerStage * static_cast<float>(stage_);
	return t < tuning_.minTolerance ? tuning_.minTolerance : t;
}

float StackGame::AlignmentError() const
{
	if (!hasActive_)
	{
		return 0.0f;
	}
	return AxisPos(active_) - AxisPos(Top());
}

void StackGame::SpawnActive()
{
	const Block& top = Top();
	active_ = top;
	active_.floor = top.floor + 1;
	active_.stage = StageOfFloor(active_.floor);
	axis_ = (active_.floor % 2 == 1) ? kAxisX : kAxisY;
	const float side = ((active_.floor / 2) % 2 == 0) ? -1.0f : 1.0f;
	AxisPos(active_) = AxisPos(top) + side * tuning_.travelRange;
	dir_ = -side;
	activeAge_ = 0.0f;
	hasActive_ = true;
	autoPrevError_ = AlignmentError() - autoTarget_;

	Event e;
	e.type = EventType::Spawn;
	e.block = active_;
	e.axis = axis_;
	Push(e);
}

void StackGame::Update(float dt)
{
	if (!(dt > 0.0f))
	{
		return;
	}
	if (dt > 0.1f)
	{
		dt = 0.1f;
	}

	if (phase_ == Phase::Title && attractResetTimer_ >= 0.0f)
	{
		attractResetTimer_ -= dt;
		if (attractResetTimer_ < 0.0f)
		{
			EnterTitle();
		}
		return;
	}

	if (!hasActive_ || (phase_ != Phase::Playing && phase_ != Phase::Title))
	{
		return;
	}

	activeAge_ += dt;
	const float centre = AxisPos(Top());
	const float range = tuning_.travelRange;
	float pos = AxisPos(active_) + dir_ * CurrentSpeed() * dt;
	// Ping-pong between centre - range and centre + range.
	for (int guard = 0; guard < 4; ++guard)
	{
		if (pos > centre + range)
		{
			pos = 2.0f * (centre + range) - pos;
			dir_ = -1.0f;
		}
		else if (pos < centre - range)
		{
			pos = 2.0f * (centre - range) - pos;
			dir_ = 1.0f;
		}
		else
		{
			break;
		}
	}
	AxisPos(active_) = pos;

	if (phase_ == Phase::Title)
	{
		UpdateAutopilot(dt);
	}
}

void StackGame::PickAutopilotTarget()
{
	if (NextRandom01() < tuning_.attractPerfectChance)
	{
		autoTarget_ = 0.0f;
	}
	else
	{
		const float magnitude = 0.25f + NextRandom01() * 0.65f;
		autoTarget_ = (NextRandom01() < 0.5f ? -1.0f : 1.0f) * magnitude;
	}
}

void StackGame::UpdateAutopilot(float /*dt*/)
{
	const float err = AlignmentError() - autoTarget_;
	const bool crossed = (err <= 0.0f) != (autoPrevError_ <= 0.0f);
	autoPrevError_ = err;
	if (activeAge_ > 0.45f && crossed)
	{
		Drop();
	}
}

bool StackGame::Tap()
{
	if (phase_ != Phase::Playing || !hasActive_)
	{
		return false;
	}
	Drop();
	return true;
}

void StackGame::Drop()
{
	const Block top = Top();
	Block placed = active_;
	const float delta = AxisPos(active_) - AxisPos(top);
	const float absDelta = std::fabs(delta);
	const float size = AxisSize(active_);
	const float overlap = size - absDelta;
	hasActive_ = false;

	if (overlap <= 0.0f)
	{
		Event miss;
		miss.type = EventType::Missed;
		miss.piece = active_;
		miss.axis = axis_;
		Push(miss);
		combo_ = 0;
		if (phase_ == Phase::Title)
		{
			attractResetTimer_ = tuning_.attractResetDelay;
		}
		else
		{
			phase_ = Phase::GameOver;
			Event over;
			over.type = EventType::GameOver;
			over.stage = stage_;
			Push(over);
		}
		return;
	}

	const bool perfect = absDelta <= CurrentTolerance();
	bool grew = false;
	if (perfect)
	{
		AxisPos(placed) = AxisPos(top);
		++combo_;
		++perfects_;
		if (combo_ >= tuning_.growComboThreshold && AxisSize(placed) < tuning_.baseSize)
		{
			float grown = AxisSize(placed) + tuning_.growAmount;
			if (grown > tuning_.baseSize)
			{
				grown = tuning_.baseSize;
			}
			AxisSize(placed) = grown;
			grew = true;
		}
	}
	else
	{
		combo_ = 0;
		AxisSize(placed) = overlap;
		AxisPos(placed) = AxisPos(top) + delta * 0.5f;

		Block piece = active_;
		AxisSize(piece) = absDelta;
		AxisPos(piece) = AxisPos(placed) + Sign(delta) * (overlap * 0.5f + absDelta * 0.5f);
		Event sliced;
		sliced.type = EventType::Sliced;
		sliced.piece = piece;
		sliced.axis = axis_;
		Push(sliced);
	}

	int gain = 1;
	if (perfect)
	{
		gain += combo_ < 5 ? combo_ : 5;
	}
	if (phase_ == Phase::Playing)
	{
		score_ += gain;
	}
	tower_.push_back(placed);

	Event landed;
	landed.type = EventType::Placed;
	landed.block = placed;
	landed.axis = axis_;
	landed.perfect = perfect;
	landed.combo = combo_;
	landed.scoreGain = gain;
	landed.stage = stage_;
	Push(landed);

	if (grew)
	{
		Event g;
		g.type = EventType::Grew;
		g.block = placed;
		g.axis = axis_;
		g.combo = combo_;
		Push(g);
	}

	if (phase_ == Phase::Title)
	{
		PickAutopilotTarget();
		if (placed.floor >= tuning_.attractMaxFloors)
		{
			attractResetTimer_ = tuning_.attractResetDelay;
			return;
		}
	}
	else if (placed.floor >= StageGoalFloor())
	{
		// Stage clear: restore the top block to full size as a reward.
		Block& restored = tower_.back();
		restored.sx = tuning_.baseSize;
		restored.sy = tuning_.baseSize;
		Event clear;
		clear.type = EventType::StageClear;
		clear.block = restored;
		clear.stage = stage_;
		Push(clear);

		if (stage_ + 1 >= kStageCount)
		{
			phase_ = Phase::Won;
			Event won;
			won.type = EventType::Won;
			won.stage = stage_;
			Push(won);
			return;
		}
		++stage_;
	}

	SpawnActive();
}

// ---------------------------------------------------------------------------
// Procedural audio
// ---------------------------------------------------------------------------

namespace
{

struct SynthNoise
{
	uint32_t state = 0x9E3779B9u;
	float Next()
	{
		state = state * 1664525u + 1013904223u;
		return static_cast<float>(state >> 8) / 8388608.0f - 1.0f;
	}
};

float SemitoneToHz(float baseHz, float semitones)
{
	return baseHz * std::pow(2.0f, semitones / 12.0f);
}

// Adds a decaying bell-like tone starting at startSec.
void AddBell(std::vector<float>& out, int sampleRate, float startSec, float hz, float amp, float decaySec)
{
	const int start = static_cast<int>(startSec * static_cast<float>(sampleRate));
	const float sr = static_cast<float>(sampleRate);
	const float partials[3][2] = {{1.0f, 1.0f}, {2.0f, 0.35f}, {3.01f, 0.12f}};
	for (int i = start; i < static_cast<int>(out.size()); ++i)
	{
		const float t = static_cast<float>(i - start) / sr;
		const float attack = t < 0.004f ? t / 0.004f : 1.0f;
		const float env = attack * std::exp(-t / decaySec);
		if (env < 0.0005f && t > 0.01f)
		{
			break;
		}
		float s = 0.0f;
		for (const auto& p : partials)
		{
			s += p[1] * std::sin(2.0f * kLumiPi * hz * p[0] * t) * std::exp(-t * (p[0] - 1.0f) * 2.5f);
		}
		out[static_cast<size_t>(i)] += amp * env * s;
	}
}

void Finalize(std::vector<float>& out)
{
	// Soft clip and short fade-out to avoid clicks.
	const size_t n = out.size();
	const size_t fade = n < 256 ? n : 256;
	for (size_t i = 0; i < n; ++i)
	{
		float s = std::tanh(out[i] * 1.1f) * 0.92f;
		if (i >= n - fade)
		{
			s *= static_cast<float>(n - i) / static_cast<float>(fade);
		}
		out[i] = s;
	}
}

}  // namespace

std::vector<float> SynthesizeSfx(Sfx sfx, int param, int sampleRate)
{
	if (sampleRate < 8000)
	{
		sampleRate = 8000;
	}
	const float sr = static_cast<float>(sampleRate);
	std::vector<float> out;
	SynthNoise noise;

	// Major pentatonic degrees in semitones.
	const float pentatonic[5] = {0.0f, 2.0f, 4.0f, 7.0f, 9.0f};
	const float c5 = 523.25f;

	switch (sfx)
	{
	case Sfx::Start:
	{
		out.assign(static_cast<size_t>(0.16f * sr), 0.0f);
		AddBell(out, sampleRate, 0.0f, SemitoneToHz(c5, 7.0f), 0.35f, 0.05f);
		AddBell(out, sampleRate, 0.05f, SemitoneToHz(c5, 12.0f), 0.35f, 0.07f);
		break;
	}
	case Sfx::Place:
	{
		out.assign(static_cast<size_t>(0.14f * sr), 0.0f);
		const float pitch = 150.0f + static_cast<float>(param % 5) * 6.0f;
		float lp = 0.0f;
		for (size_t i = 0; i < out.size(); ++i)
		{
			const float t = static_cast<float>(i) / sr;
			const float body = std::sin(2.0f * kLumiPi * pitch * t) * std::exp(-t / 0.035f) +
			                   0.5f * std::sin(2.0f * kLumiPi * pitch * 2.7f * t) * std::exp(-t / 0.018f);
			lp += (noise.Next() - lp) * 0.25f;
			const float click = lp * std::exp(-t / 0.006f);
			out[i] = 0.75f * body + 0.9f * click;
		}
		break;
	}
	case Sfx::Perfect:
	{
		out.assign(static_cast<size_t>(0.75f * sr), 0.0f);
		int step = param - 1;
		if (step < 0)
		{
			step = 0;
		}
		if (step > 14)
		{
			step = 14;
		}
		const float semis = pentatonic[step % 5] + 12.0f * static_cast<float>(step / 5);
		const float hz = SemitoneToHz(c5, semis);
		AddBell(out, sampleRate, 0.0f, hz, 0.45f, 0.28f);
		AddBell(out, sampleRate, 0.0f, hz * 2.0f, 0.12f, 0.12f);
		// A soft body knock underneath so the landing still reads as physical.
		for (size_t i = 0; i < out.size() && i < static_cast<size_t>(0.08f * sr); ++i)
		{
			const float t = static_cast<float>(i) / sr;
			out[i] += 0.35f * std::sin(2.0f * kLumiPi * 170.0f * t) * std::exp(-t / 0.03f);
		}
		break;
	}
	case Sfx::Grow:
	{
		out.assign(static_cast<size_t>(0.45f * sr), 0.0f);
		for (int k = 0; k < 4; ++k)
		{
			AddBell(out, sampleRate, 0.045f * static_cast<float>(k), SemitoneToHz(c5, 12.0f + pentatonic[(k * 2) % 5] + (k >= 3 ? 12.0f : 0.0f)), 0.18f, 0.12f);
		}
		break;
	}
	case Sfx::StageClear:
	{
		out.assign(static_cast<size_t>(1.4f * sr), 0.0f);
		const float root = static_cast<float>(param % 3) * 2.0f;  // vary key per stage
		const float chord[5] = {0.0f, 4.0f, 7.0f, 12.0f, 16.0f};
		for (int k = 0; k < 5; ++k)
		{
			AddBell(out, sampleRate, 0.085f * static_cast<float>(k), SemitoneToHz(c5 * 0.5f, root + chord[k]), 0.32f, 0.45f);
		}
		AddBell(out, sampleRate, 0.45f, SemitoneToHz(c5, root + 12.0f), 0.22f, 0.5f);
		break;
	}
	case Sfx::Miss:
	{
		out.assign(static_cast<size_t>(0.7f * sr), 0.0f);
		float phase = 0.0f;
		for (size_t i = 0; i < out.size(); ++i)
		{
			const float t = static_cast<float>(i) / sr;
			const float hz = 320.0f * std::exp(-t * 2.6f) + 55.0f;
			phase += 2.0f * kLumiPi * hz / sr;
			const float env = (t < 0.01f ? t / 0.01f : 1.0f) * std::exp(-t / 0.28f);
			out[i] = env * (0.6f * std::sin(phase) + 0.22f * std::sin(3.0f * phase));
		}
		break;
	}
	case Sfx::Win:
	{
		out.assign(static_cast<size_t>(2.6f * sr), 0.0f);
		const float melody[8] = {0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 12.0f, 16.0f, 19.0f};
		for (int k = 0; k < 8; ++k)
		{
			AddBell(out, sampleRate, 0.11f * static_cast<float>(k), SemitoneToHz(c5 * 0.5f, melody[k]), 0.28f, 0.35f);
		}
		const float chord[4] = {0.0f, 4.0f, 7.0f, 12.0f};
		for (int k = 0; k < 4; ++k)
		{
			AddBell(out, sampleRate, 0.95f, SemitoneToHz(c5 * 0.5f, chord[k]), 0.22f, 0.9f);
		}
		break;
	}
	}

	Finalize(out);
	return out;
}

std::vector<int16_t> ToPcm16(const std::vector<float>& samples)
{
	std::vector<int16_t> pcm(samples.size());
	for (size_t i = 0; i < samples.size(); ++i)
	{
		float s = samples[i];
		s = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
		pcm[i] = static_cast<int16_t>(std::lrint(s * 32767.0f));
	}
	return pcm;
}

}  // namespace lumi
