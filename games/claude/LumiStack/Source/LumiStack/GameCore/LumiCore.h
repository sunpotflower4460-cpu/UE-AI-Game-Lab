// Lumi Stack — engine-independent game core.
//
// This file intentionally has no Unreal Engine dependency. The exact same rules are
// compiled into the UE module, into the native unit tests (CoreTests/) and into the
// WebAssembly preview (WebPreview/), so gameplay can be verified without the editor.
//
// Units: 1 core unit == 100 Unreal units (1 m). Floors are stacked along +Z.
#pragma once

#include <cstdint>
#include <vector>

namespace lumi
{

constexpr int kAxisX = 0;
constexpr int kAxisY = 1;

// Colour in sRGB space, components 0..1.
struct Rgb
{
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
};

struct Block
{
	float cx = 0.0f;  // centre X
	float cy = 0.0f;  // centre Y
	float sx = 0.0f;  // size X
	float sy = 0.0f;  // size Y
	int floor = 0;    // 0 == pedestal
	int stage = 0;    // stage the floor belongs to (drives colour)
};

enum class Phase : uint8_t
{
	Title,     // attract mode: an autopilot builds a demo tower
	Playing,
	GameOver,
	Won,
};

enum class EventType : uint8_t
{
	Reset,       // tower rebuilt from scratch (only the pedestal remains)
	Spawn,       // a new sliding block appeared             (block, axis)
	Placed,      // the sliding block landed                 (block, perfect, combo, scoreGain)
	Sliced,      // the overhang was cut off and falls        (piece, axis)
	Grew,        // perfect streak made the top block grow    (block)
	StageClear,  // stage goal reached, top block restored    (block, stage = cleared stage)
	Missed,      // the sliding block missed entirely         (piece)
	GameOver,
	Won,
};

struct Event
{
	EventType type = EventType::Reset;
	Block block;
	Block piece;
	int axis = kAxisX;
	int combo = 0;
	int stage = 0;
	int scoreGain = 0;
	bool perfect = false;
};

struct StageInfo
{
	const char* name;
	int floors;       // floors to build in this stage
	Rgb skyTop;
	Rgb skyBottom;
	float hueStart;   // degrees
	float hueStep;    // degrees per floor
	float saturation;
	float value;
};

int StageCount();
const StageInfo& GetStage(int stageIndex);  // index is clamped
int TotalFloorsToWin();

Rgb HsvToRgb(float hueDegrees, float saturation, float value);
Rgb LerpRgb(const Rgb& a, const Rgb& b, float t);
Rgb ScaleRgb(const Rgb& c, float s);
Rgb BlockColor(const Block& block);
Rgb PedestalColor();

struct Tuning
{
	float baseSize = 3.2f;
	float blockHeight = 0.5f;
	float travelRange = 4.6f;
	float baseSpeed = 2.4f;
	float speedPerStage = 0.5f;
	float speedPerFloor = 0.03f;
	float perfectTolerance = 0.13f;
	float tolerancePerStage = 0.015f;
	float minTolerance = 0.06f;
	int growComboThreshold = 3;
	float growAmount = 0.15f;
	float attractResetDelay = 1.6f;
	int attractMaxFloors = 30;
	float attractPerfectChance = 0.72f;
};

class StackGame
{
public:
	explicit StackGame(uint32_t seed = 0x2545F491u, const Tuning& tuning = Tuning());

	// Enter attract mode (title screen). The autopilot builds a demo tower.
	void EnterTitle();
	// Start a real run from scratch.
	void StartRun();
	// Advance the simulation. dt is clamped internally.
	void Update(float dt);
	// Player tap. Returns true when a block was dropped.
	bool Tap();

	Phase GetPhase() const { return phase_; }
	const std::vector<Block>& Tower() const { return tower_; }
	const Block& Top() const { return tower_.back(); }
	bool HasActive() const { return hasActive_; }
	const Block& Active() const { return active_; }
	int ActiveAxis() const { return axis_; }

	int Score() const { return score_; }
	int TopFloor() const { return tower_.back().floor; }
	int StageIndex() const { return stage_; }
	int Combo() const { return combo_; }
	int PerfectCount() const { return perfects_; }
	int StageStartFloor() const;
	int StageGoalFloor() const;
	float StageProgress() const;  // 0..1 within the current stage

	float CurrentSpeed() const;
	float CurrentTolerance() const;
	// Signed offset of the sliding block from perfect alignment along its axis.
	float AlignmentError() const;

	const Tuning& GetTuning() const { return tuning_; }

	// Events produced since the last call. The returned reference stays valid until
	// the next mutating call; call ClearEvents() once they have been consumed.
	const std::vector<Event>& Events() const { return events_; }
	void ClearEvents() { events_.clear(); }

private:
	void ResetTower();
	void SpawnActive();
	void Drop();
	void Push(const Event& e) { events_.push_back(e); }
	float& AxisPos(Block& b) const { return axis_ == kAxisX ? b.cx : b.cy; }
	float AxisPos(const Block& b) const { return axis_ == kAxisX ? b.cx : b.cy; }
	float& AxisSize(Block& b) const { return axis_ == kAxisX ? b.sx : b.sy; }
	float AxisSize(const Block& b) const { return axis_ == kAxisX ? b.sx : b.sy; }
	int StageOfFloor(int floor) const;
	void UpdateAutopilot(float dt);
	void PickAutopilotTarget();
	float NextRandom01();

	Tuning tuning_;
	uint32_t rng_;
	Phase phase_ = Phase::Title;
	std::vector<Block> tower_;
	std::vector<Event> events_;
	Block active_;
	bool hasActive_ = false;
	int axis_ = kAxisX;
	float dir_ = 1.0f;
	float activeAge_ = 0.0f;
	int score_ = 0;
	int stage_ = 0;
	int combo_ = 0;
	int perfects_ = 0;

	// Attract-mode autopilot state.
	float autoTarget_ = 0.0f;
	float autoPrevError_ = 0.0f;
	float attractResetTimer_ = -1.0f;
};

// ---------------------------------------------------------------------------
// Procedural audio. All sound effects are synthesised, no audio assets needed.
// ---------------------------------------------------------------------------
enum class Sfx : uint8_t
{
	Start,
	Place,       // param: floor (slight pitch variation)
	Perfect,     // param: combo (rising pentatonic pitch)
	Grow,
	StageClear,  // param: cleared stage index
	Miss,
	Win,
};

constexpr int kSfxCount = 7;

// Mono float samples in [-1, 1].
std::vector<float> SynthesizeSfx(Sfx sfx, int param, int sampleRate);
// Converts float samples to little-endian signed 16-bit PCM.
std::vector<int16_t> ToPcm16(const std::vector<float>& samples);

}  // namespace lumi
