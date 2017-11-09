#pragma once

#include <vector>
#include <array>
#include <tuple>
#include <cstdint>
#include <climits>
#include "pcg/pcg_random.hpp"
#include "note.h"
#include "skill.h"
#include "util.h"


class Live {
public:
	bool prepare(const char * json);
	int simulate(int id, uint64_t seed = UINT64_C(0xcafef00dd15ea5e5));

public:
	static constexpr double FRAME_TIME = 0.016;
	static constexpr double PERFECT_WINDOW = 0.032;
	static constexpr double GREAT_WINDOW = 0.080;
	static constexpr double GOOD_WINDOW = 0.128;
	static constexpr std::array<std::pair<int, double>, 7> COMBO_MUL = { {
		{50, 1},
		{100, 1.1},
		{200, 1.15},
		{400, 1.2},
		{600, 1.25},
		{800, 1.3},
		{INT_MAX, 1.35},
	} };

private:
	template <class JsonValue>
	void loadSettings(const JsonValue & jsonObj);

	template <class JsonValue>
	void loadUnit(const JsonValue & jsonObj);

	template <class JsonValue>
	void loadChart(const JsonValue & jsonObj);

	void initSimulation();

	void simulateHitError();

	double computeScore();

private:
	struct Hit {
		double time;
		int noteIndex;
		bool isPerfect;
		bool isHoldBegin;
		bool isHoldEnd;
		bool isSlide;

		Hit() = default;
		Hit(int noteIndex, const Note & note, bool isHoldEnd)
			: time(isHoldEnd ? note.holdEndTime : note.time)
			, noteIndex(noteIndex)
			, isPerfect(true)
			, isHoldBegin(note.isHold && !isHoldEnd)
			, isHoldEnd(isHoldEnd)
			, isSlide(note.isSlide) {
		}
	};

	struct TimedEvent {
		enum class Type {
			SkillOff,
			ActiveSkillOn,
			PassiveSkillOn,
		};

		double time;
		Type type;
		unsigned id;

		bool operator <(const TimedEvent & b) const {
			return std::tie(time, type, id) < std::tie(b.time, b.type, b.id);
		}
	};

private:
	pcg32 rng;

	// Settings
	double hiSpeed;
	double judgeOffset;
	double sigmaHit;
	double sigmaHoldBegin;
	double sigmaHoldEnd;
	double sigmaSlide;
	double gRateHit;
	double gRateHoldBegin;
	double gRateHoldEnd;
	double gRateSlide;
	double gRateSlideHoldEnd;

	// Unit
	double strength;
	std::vector<int> attributes;
	int skillNum;
	std::vector<Skill> skills;

	// Chart
	int noteNum;
	std::vector<Note> notes;

	// Simulation
	double score;
	int note;
	int combo;
	int perfect;
	int starPerfect;
	int judgeCount;
	size_t hitIndex;
	decltype(COMBO_MUL)::const_iterator itComboMul;
	std::vector<Hit> hits;
	MinPriorityQueue<TimedEvent> timedEvents;
	std::vector<unsigned> skillIds;
};
