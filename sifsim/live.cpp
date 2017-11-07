#include "live.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "pcg/pcg_random.hpp"
#include <random>
#include <cmath>

using namespace std;
using namespace rapidjson;

constexpr double SQRT1_2 = 0.70710678118654752;


bool Live::prepare(const char * json) {
	Document doc;
	if (doc.Parse(json).HasParseError()) return false;

	// Settings
	sigmaHit = 0.015;
	sigmaHoldBegin = 0.015;
	sigmaHoldEnd = 0.018;
	sigmaSlide = 0.030;
	gRateHoldBegin = erfc(PERFECT_WINDOW / sigmaHoldBegin * SQRT1_2);
	gRateSlide = erfc(GREAT_WINDOW / sigmaSlide * SQRT1_2);

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
		notes[i] = t - 0.7;
		if (note.isHold) {
			if (!doc[i]["effect_value"].IsDouble()) return false;
			t += doc[i]["effect_value"].GetDouble();
		}
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


int Live::simulate(int id) {
	pcg32 rng;
	rng.advance(static_cast<uint64_t>(id) << 32);
	normal_distribution<> eHit(0, sigmaHit);
	normal_distribution<> eHoldEnd(0, sigmaHoldEnd);
	normal_distribution<> eSlide(0, sigmaSlide);
	bernoulli_distribution gHoldBegin(gRateHoldBegin);
	bernoulli_distribution gSlide(gRateSlide);
	//bernoulli_distribution g(0.05);
	for (auto & note : combos) {
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
			if (fabs(e) > GOOD_WINDOW) {
				e = copysign(GOOD_WINDOW, e);
			}
			note.gr = fabs(e) > GREAT_WINDOW;
		} else {
			if (fabs(e) > GREAT_WINDOW) {
				e = copysign(GREAT_WINDOW, e);
			}
			note.gr = fabs(e) > PERFECT_WINDOW;
		}
		note.hitTime += e;
		//note.gr = g(rng);
		//note.grBegin = g(rng);
	}

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
			score = floor(score / 100.);
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
