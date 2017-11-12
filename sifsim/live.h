#pragma once

#include <vector>
#include <array>
#include <tuple>
#include <cstdint>
#include <climits>
#include "pcg/pcg_random.hpp"
#include "rapidjson/document.h"
#include "note.h"
#include "card.h"
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
	struct LiveNote;
	struct LiveCard;

	void loadSettings(const rapidjson::Value & jsonObj);
	void loadUnit(const rapidjson::Value & jsonObj);
	void loadCharts(const rapidjson::Value & jsonObj);
	void processCharts();

	void initSimulation();
	void initNextSong();
	void shuffleSkills();
	void initSkills();
	void initSkillsForNextSong();

	void simulateHitError();
	void startSkillTrigger();
	double computeScore(const LiveNote & note, bool isPerfect) const;

	void skillTrigger(LiveCard & card);
	void skillOn(LiveCard & card);
	void skillOff(LiveCard & card);
	void skillSetNextTrigger(LiveCard & card);
	void skillSetNextTriggerOnNextFrame(LiveCard & card);
	void updateChain(const LiveCard & card);
	void updateLastSkill(const LiveCard & card);

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
			, isSlide(note.isSlide) {}
	};

	struct LiveNote : public Note {
		bool isHoldBeginPerfect;
		double holdBeginHitTime;
	};

	struct LiveCard : public Card {
		unsigned skillId;
		int currentSkillLevel;
		bool isActive;
		int nextTrigger;
		int remainingTriggerType;
		std::vector<int> triggerStatus;

		const Skill::LevelData & skillLevel() const {
			return skill.levels[currentSkillLevel - 1];
		}
	};

	struct LiveChart {
		int noteNum;
		int beginNote;
		int endNote;
		double lastNoteShowTime;
		std::vector<LiveNote> notes;
	};

	enum SkillIdFlags : unsigned {
		SkillEventMask      = 0xf00000,
		SkillOff            = 0x100000,
		SkillNextTrigger    = 0x200000,
		SkillOn             = 0x300000,
		SkillPriorityMask   = 0xf0000,
		ActiveSkill         = 0x10000,
		PassiveSkill        = 0x20000,
		SkillOrderMask      = 0xff00,
		SkillIndexMask      = 0xff,
	};
	static constexpr int SKILL_ORDER_SHIFT = 8;

	struct SkillEvent {
		double time;
		unsigned id;

		SkillEvent() = default;
		SkillEvent(double time, unsigned id) : time(time), id(id) {}
		bool operator <(const SkillEvent & b) const {
			return std::tie(time, id) < std::tie(b.time, b.id);
		}
	};

	template <class T = int>
	struct SkillTrigger {
		T value;
		unsigned id;

		SkillTrigger() = default;
		SkillTrigger(T value, unsigned id) :value(value), id(id) {}
		bool operator <(const SkillTrigger & b) const {
			return std::tie(value, id) < std::tie(b.value, b.id);
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
	double status;
	int cardNum;
	std::vector<LiveCard> cards;

	// Chart
	int chartNum;
	std::vector<LiveChart> charts;
	int memberCategory;

	// Simulation
	int chartIndex;
	double time;
	size_t hitIndex;
	double score;
	int combo;
	int perfect;
	int starPerfect;
	int judgeCount;
	decltype(COMBO_MUL)::const_iterator itComboMul;
	std::vector<std::vector<Hit>> chartHits;
	std::vector<double> combos;
	MinPriorityQueue<SkillEvent> skillEvents;
	MinPriorityQueue<SkillTrigger<double>> scoreTriggers;
	MinPriorityQueue<SkillTrigger<>> perfectTriggers;
	MinPriorityQueue<SkillTrigger<>> starPerfectTriggers;
	std::vector<int> chainTriggers;
};
