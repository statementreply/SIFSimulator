#include "live.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "pcg/pcg_random.hpp"
#include <random>
#include <cmath>

using namespace std;
using namespace rapidjson;

#ifndef USE_FAST_RANDOM
#define USE_FAST_RANDOM 1
#endif

#ifndef SIMULATE_HIT_ERROR
#define SIMULATE_HIT_ERROR 1
#endif

#if USE_FAST_RANDOM
#include "fastrandom.h"
namespace Random = FastRandom;
#else
namespace Random = std;
#endif

constexpr double SQRT1_2 = 0.70710678118654752;


bool Live::prepare(const char * json) {
	Document doc;
	if (doc.Parse(json).HasParseError()) return false;

	// Settings
	hiSpeed = 0.7;
	hitOffset = 0.;
	sigmaHit = 0.015;
	sigmaHoldBegin = 0.015;
	sigmaHoldEnd = 0.018;
	sigmaSlide = 0.030;
	gRateHit = erfc(PERFECT_WINDOW / sigmaHit * SQRT1_2);
	gRateHoldBegin = erfc(PERFECT_WINDOW / sigmaHoldBegin * SQRT1_2);
	gRateHoldEnd = erfc(PERFECT_WINDOW / sigmaHoldEnd * SQRT1_2);
	gRateSlide = erfc(GREAT_WINDOW / sigmaSlide * SQRT1_2);
	gRateSlideHoldEnd = erfc(GREAT_WINDOW / sigmaHoldEnd * SQRT1_2);

	// Unit
	attr = 63918;

	// Chart
	if (!doc.IsArray()) return false;
	unsigned noteNum = doc.Size();
	combos.resize(noteNum);
	notes.resize(noteNum);
	for (unsigned i = 0; i < noteNum; i++) {
		if (!doc[i].IsObject()) return false;
		auto & note = combos[i];
		if (!doc[i]["position"].IsInt()) return false;
		note.position = doc[i]["position"].GetInt();
		if (!doc[i]["effect"].IsInt()) return false;
		note.effect(doc[i]["effect"].GetInt());
		if (!doc[i]["timing_sec"].IsDouble()) return false;
		double t = doc[i]["timing_sec"].GetDouble();
		// SIF built-in offset :<
		t -= 0.1;
		notes[i] = t - hiSpeed;
		if (note.isHold) {
			if (!doc[i]["effect_value"].IsDouble()) return false;
			t += doc[i]["effect_value"].GetDouble();
		}
		note.time = t;
		note.hitTime = t;
	}

	return true;
}


namespace {
	struct TimedEvent {
		enum class Type {
			Hit,
			SkillOff,
			ActiveSkillOn,
			PassiveSkillOn,
		};

		double time;
		Type type;
		int id;

		bool operator <(const TimedEvent & b) const {
			return std::tie(time, type, id) < std::tie(b.time, b.type, b.id);
		}
	};
}


int Live::simulate(int id, uint64_t seed) {
	pcg32 rng(seed);
	rng.advance(static_cast<uint64_t>(id) << 32);

	// Hit accuracy
#if SIMULATE_HIT_ERROR
	Random::normal_distribution<> eHit(0, sigmaHit);
	Random::normal_distribution<> eHoldEnd(0, sigmaHoldEnd);
	Random::normal_distribution<> eSlide(0, sigmaSlide);
	Random::bernoulli_distribution gHoldBegin(gRateHoldBegin);
	Random::bernoulli_distribution gSlide(gRateSlide);
	for (auto && note : combos) {
		double e;
		if (note.isHold) {
			e = eHoldEnd(rng);
			if (note.isSlide) {
				note.grBegin = gSlide(rng);
			} else {
				note.grBegin = gHoldBegin(rng);
			}
		} else {
			if (note.isSlide) {
				e = eSlide(rng);
			} else {
				e = eHit(rng);
			}
			note.grBegin = false;
		}
		if (note.isSlide) {
			if (!(fabs(e) < GOOD_WINDOW)) {
				e = copysign(GOOD_WINDOW, e);
			}
			note.gr = !(fabs(e) < GREAT_WINDOW);
		} else {
			if (!(fabs(e) < GREAT_WINDOW)) {
				e = copysign(GREAT_WINDOW, e);
			}
			note.gr = !(fabs(e) < PERFECT_WINDOW);
		}
		note.hitTime = note.time + hitOffset + e;
	}
	sort(combos.begin(), combos.end(), [](auto && a, auto && b) {
		return a.hitTime < b.hitTime;
	});
#else
	Random::bernoulli_distribution gHit(gRateHit);
	Random::bernoulli_distribution gHoldBegin(gRateHoldBegin);
	Random::bernoulli_distribution gHoldEnd(gRateHoldEnd);
	Random::bernoulli_distribution gSlide(gRateSlide);
	Random::bernoulli_distribution gSlideHoldEnd(gRateSlideHoldEnd);
	for (auto && note : combos) {
		if (note.isHold) {
			if (note.isSlide) {
				note.gr = gSlideHoldEnd(rng);
				note.grBegin = gSlide(rng);
			} else {
				note.gr = gHoldEnd(rng);
				note.grBegin = gHoldBegin(rng);
			}
		} else {
			if (note.isSlide) {
				note.gr = gSlide(rng);
			} else {
				note.gr = gHit(rng);
			}
			note.grBegin = false;
		}
	}
#endif

	double totalScore = 0;
	size_t combo = 0;
	auto itComboMul = COMBO_MUL.cbegin();
	while (combo < combos.size()) {
		if (true) {
			const auto & note = combos[combo];
			double score = attr;
			score *= note.gr ? 1.1 : 1.25;
			score *= itComboMul->second;
			// Doesn't judge accuracy?
			// L7_84 = L12_12.SkillEffect.PerfectBonus.apply(L7_84)
			if (false) { // A0_77.live_member_category == A2_79.member_category
				score *= 1.1;
			}
			if (note.isHold) {
				score *= note.grBegin ? 1.1 : 1.25;
			}
			if (note.isSlide) {
				score *= 0.5;
			}
			if (true) { // A1_78.color == A2_79.attribute
				score *= 1.1;
			}
			score /= 100.;
			score = floor(score);
			if (!note.gr) { // Only judge hold end accuracy?
				// L7_84 = L7_84 + L12_12.SkillEffect.PerfectBonus.sumBonus(A3_80)
			}
			// L7_84 = L12_12.Combo.applyFixedValueBonus(L7_84)
			// L8_117 = L19_19.SkillEffect.ScoreBonus.apply(L9_118)
			// L9_118 = L7_116 * bonus_score_rate
			score = ceil(score);
			totalScore += score;
			++combo;
			if (combo >= itComboMul->first) {
				++itComboMul;
			}
		}
	}

	return static_cast<int>(totalScore);
}
