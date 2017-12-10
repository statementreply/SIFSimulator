#pragma once
#include "configure.h"

#include <vector>
#include <array>
#include <deque>
#include <string>
#include <tuple>
#include "optional.h"
#include <cstdint>
#include <climits>
#include <cstdio>
#include "pcg/pcg_random.hpp"
#include "rapidjson/document.h"
#include "note.h"
#include "card.h"
#include "util.h"

#if USE_FAST_RANDOM
#include "fastrandom.h"
using FastRandom::BernoulliDistribution;
using FastRandom::NormalDistribution;
#else
#include <random>
using BernoulliDistribution = std::bernoulli_distribution;
template <class RealType = double>
using NormalDistribution = std::normal_distribution<RealType>;
#endif


class Live {
public:
	explicit Live(FILE * fp);
	int simulate(uint64_t id, uint64_t seed = UINT64_C(0xcafef00dd15ea5e5));

public:
	static constexpr double FRAME_TIME = 0.016;
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

	void loadSettings(const rapidjson::Value & json);
	void loadLiveBonus(const rapidjson::Value & json);
	void loadSkillOrder(const rapidjson::Value & json);
	void loadUnit(const rapidjson::Value & json);
	void loadCharts(const rapidjson::Value & json);
	void processUnit();
	void processCharts();

	void loadHitError(const rapidjson::Value & json);
#if !SIMULATE_HIT_TIMING
	void loadGreatRate(const rapidjson::Value & json);
#endif

	void initSimulation();
	void initNextSong();
	void initForEverySong();
	void shuffleSkills();
	void initSkills();
	void initSkillsForNextSong();
	void initSkillsForEverySong();

	void simulateHitError();
	void startSkillTrigger();
	double computeScore(const LiveNote & note, bool isPerfect) const;

	void skillTrigger(LiveCard & card);
	void skillOn(LiveCard & card, bool isMimic);
	void skillOff(LiveCard & card);
	void skillSetNextTrigger(LiveCard & card);
	void skillSetNextTriggerOnNextFrame(LiveCard & card);
	void updateChain(const LiveCard & otherCard);
	void updateMimic(const LiveCard & otherCard);
	bool getMimic(LiveCard & card);

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
		int remainingChainTypeNum;
		std::vector<int> chainStatus;
		int mimicSkillIndex;
		int mimicSkillLevel;
		optional<double> buffedStatus;
		optional<double> syncStatus;

		const Skill::LevelData & skillLevel() const {
			return skill.levels[currentSkillLevel - 1];
		}

		double getSyncStatus() const {
			return syncStatus.value_or(buffedStatus.value_or(status));
		}
	};

	struct LiveChart {
		int memberCategory;
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

	enum class LiveMode {
		Normal = 0,
		MF = 3,
	};

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

	struct MimicStack {
		double pushTime;
		double popTime;
		unsigned skillId;
		int skillLevel;
	};

private:
	pcg32 rng;

	// Settings
	LiveMode mode = LiveMode::Normal;
	double hiSpeed = 0.7;
	double judgeOffset = 0;
	double hitPerfectWindow = 0.032;
	double hitGreatWindow = 0.080;
	double slidePerfectWindow = hitGreatWindow;
	double slideGreatWindow = 0.128;
#if SIMULATE_HIT_TIMING
	NormalDistribution<> eHit{ 0, 0.015 };
	NormalDistribution<> eHoldBegin{ 0, 0.015 };
	NormalDistribution<> eHoldEnd{ 0, 0.015 };
	NormalDistribution<> eSlide{ 0, 0.015 };
#else
	BernoulliDistribution gHit{ 0.05 };
	BernoulliDistribution gHoldBegin{ 0.05 };
	BernoulliDistribution gHoldEnd{ 0.05 };
	BernoulliDistribution gSlide{ 0.05 };
	BernoulliDistribution gSlideHoldEnd{ 0.05 };
#endif

	// Live bonus
	double liveScoreRate = 1;
	double liveActivationRate = 1;

	// Skill order
	std::vector<int> skillOrder;

	// Unit
	double unitStatus = 0;
	double judgeSisStatus = 0;
	std::vector<LiveCard> cards;

	// Chart
	std::vector<LiveChart> charts;

	// Simulation
	// Basic
	double status = 0;
	size_t chartIndex = 0;
	int chartMemberCategory = 0;
	double chartScoreRate = 1;
	double chartActivationRate = 1;
	double time = 0;
	int hitIndex = 0;
	double score = 0;
	int combo = 0;
	int perfect = 0;
	int starPerfect = 0;
	decltype(COMBO_MUL)::const_iterator itComboMul = COMBO_MUL.begin();

	// Pre calc
	std::vector<std::vector<Hit>> chartHits;
	std::vector<double> combos;

	// Skill trigger
	MinPriorityQueue<SkillEvent> skillEvents;
	MinPriorityQueue<SkillTrigger<double>> scoreTriggers;
	MinPriorityQueue<SkillTrigger<>> perfectTriggers;
	MinPriorityQueue<SkillTrigger<>> starPerfectTriggers;
	std::vector<int> chainTriggers;
	MimicStack mimicStack{ -1, 0, 0, 0 };

	// Skill effect
	int judgeCount = 0;
	double activationMod = 1;
	std::deque<double> perfectBonusFixedQueue;
	double perfectBonusFixed = 0;
	std::deque<double> perfectBonusRateQueue;
	double perfectBonusRate = 1;
};
