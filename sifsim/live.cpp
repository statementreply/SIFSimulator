#include "live.h"
#include "util.h"
#include "rapidjson/document.h"
#include "rapidjsonutil.h"
#include <cmath>
#include <queue>
#include <stdexcept>

using namespace std;

#ifndef USE_FAST_RANDOM
#define USE_FAST_RANDOM 1
#endif

#ifndef SIMULATE_HIT_TIMING
#define SIMULATE_HIT_TIMING 1
#endif

#if USE_FAST_RANDOM
#include "fastrandom.h"
using namespace FastRandom;
#else
#include <random>
#endif

constexpr double SQRT1_2 = 0.70710678118654752;


bool Live::prepare(const char * json) {
	try {
		rapidjson::Document doc;
		if (doc.Parse(json).HasParseError()) {
			throw runtime_error("Invalid JSON");
		}
		loadSettings(doc);
		loadUnit(doc);
		loadChart(doc);
	} catch (const runtime_error &) {
		return false;
	}
	return true;
}


template <class JsonValue>
void Live::loadSettings(const JsonValue & jsonObj) {
	hiSpeed = 0.7;
	judgeOffset = 0.;
	sigmaHit = 0.015;
	sigmaHoldBegin = 0.015;
	sigmaHoldEnd = 0.018;
	sigmaSlide = 0.030;
	gRateHit = erfc(PERFECT_WINDOW / sigmaHit * SQRT1_2);
	gRateHoldBegin = erfc(PERFECT_WINDOW / sigmaHoldBegin * SQRT1_2);
	gRateHoldEnd = erfc(PERFECT_WINDOW / sigmaHoldEnd * SQRT1_2);
	gRateSlide = erfc(GREAT_WINDOW / sigmaSlide * SQRT1_2);
	gRateSlideHoldEnd = erfc(GREAT_WINDOW / sigmaHoldEnd * SQRT1_2);
}



template <class JsonValue>
void Live::loadUnit(const JsonValue & jsonObj) {
	strength = 63918;
	attributes.resize(9, 1);
}


template <class JsonValue>
void Live::loadChart(const JsonValue & jsonObj) {
	noteNum = jsonObj.Size();
	notes.resize(noteNum);
	for (int i = 0; i < noteNum; i++) {
		auto && noteObj = GetJsonMemberObject(jsonObj, i);
		Note & note = notes[i];
		int p = GetJsonMemberInt(noteObj, "position");
		note.position = 9 - p;
		note.attribute = GetJsonMemberInt(noteObj, "notes_attribute");
		note.effect(GetJsonMemberInt(noteObj, "effect"));
		double t = GetJsonMemberDouble(noteObj, "timing_sec");
		// SIF built-in offset :<
		note.time = t - 0.1;
		note.showTime = note.time - hiSpeed;
		if (note.isHold) {
			note.holdEndTime = note.time + GetJsonMemberDouble(noteObj, "effect_value");
		} else {
			note.holdEndTime = NAN;
		}
	}
	stable_sort(notes.begin(), notes.end(), [](auto && a, auto && b) {
		return a.time < b.time;
	});

	hits.clear();
	for (int i = 0; i < noteNum; i++) {
		const Note & note = notes[i];
		hits.emplace_back(i, note, false);
		if (note.isHold) {
			hits.emplace_back(i, note, true);
		}
	}
	sort(hits.begin(), hits.end(), [](auto && a, auto && b) {
		return a.time < b.time;
	});
}


int Live::simulate(int id, uint64_t seed) {
	rng.seed(seed);
	rng.advance(static_cast<uint64_t>(id) << 32);

	initSimulation();
	simulateHitError();

	while (hitIndex < hits.size() || !timedEvents.empty()) {
		if (true || hitIndex < hits.size() && (timedEvents.empty()
			|| !(timedEvents.top().time < hits[hitIndex].time))) {
			Hit & hit = hits[hitIndex];
			Note & note = notes[hit.noteIndex];
			bool isPerfect = hit.isPerfect || judgeCount;
			if (hit.isHoldBegin) {
				note.isHoldBeginPerfect = isPerfect;
				++hitIndex;
				continue;
			}
			double noteScore = strength;
			noteScore *= isPerfect ? 1.25 : 1.1;
			noteScore *= itComboMul->second;
			// Doesn't judge accuracy?
			// L7_84 = L12_12.SkillEffect.PerfectBonus.apply(L7_84)
			if (false) { // A0_77.live_member_category == A2_79.member_category
				noteScore *= 1.1;
			}
			if (note.isHold) {
				noteScore *= note.isHoldBeginPerfect ? 1.25 : 1.1;
			}
			if (note.isSlide) {
				noteScore *= 0.5;
			}
			if (attributes[note.position] == note.attribute) {
				noteScore *= 1.1;
			}
			//noteScore = static_cast<int>(noteScore / 100.);
			noteScore = floor(noteScore / 100.);
			if (isPerfect) { // Only judge hold end accuracy?
				// L7_84 = L7_84 + L12_12.SkillEffect.PerfectBonus.sumBonus(A3_80)
			}
			// L7_84 = L12_12.Combo.applyFixedValueBonus(L7_84)
			// L8_117 = L19_19.SkillEffect.ScoreBonus.apply(L9_118)
			// L9_118 = L7_116 * bonus_score_rate
			noteScore = ceil(noteScore);
			score += noteScore;
			++hitIndex;
			++combo;
			if (combo >= itComboMul->first) {
				++itComboMul;
			}
		}
	}

	return static_cast<int>(score);
}


void Live::initSimulation() {
	score = 0;
	note = 0;
	combo = 0;
	perfect = 0;
	starPerfect = 0;
	judgeCount = 0;
	hitIndex = 0;
	itComboMul = COMBO_MUL.cbegin();
}


void Live::simulateHitError() {
#if SIMULATE_HIT_TIMING
	normal_distribution<> eHit(0, sigmaHit);
	normal_distribution<> eHoldBegin(0, sigmaHoldBegin);
	normal_distribution<> eHoldEnd(0, sigmaHoldEnd);
	normal_distribution<> eSlide(0, sigmaSlide);
	for (auto && hit : hits) {
		Note & note = notes[hit.noteIndex];
		double noteTime = hit.isHoldEnd ? note.holdEndTime : note.time;
		double judgeTime = noteTime + judgeOffset;
		double e;
		if (hit.isHoldEnd) {
			e = eHoldEnd(rng);
		} else if (hit.isSlide) {
			e = eSlide(rng);
		} else if (hit.isHoldBegin) {
			e = eHoldBegin(rng);
		} else {
			e = eHit(rng);
		}
		if (hit.isSlide) {
			if (!(fabs(e) < GOOD_WINDOW)) {
				e = copysign(GOOD_WINDOW, e);
			}
		} else {
			if (!(fabs(e) < GREAT_WINDOW)) {
				e = copysign(GREAT_WINDOW, e);
			}
		}
		if (hit.isHoldEnd) {
			double minTime = note.holdBeginHitTime + FRAME_TIME;
			double minE = minTime - judgeTime;
			if (e < minE) {
				e = minE;
			}
		}
		hit.time = judgeTime + e;
		if (hit.isSlide) {
			hit.isPerfect = (fabs(e) < GREAT_WINDOW);
		} else {
			hit.isPerfect = (fabs(e) < PERFECT_WINDOW);
		}
		if (hit.isHoldBegin) {
			note.isHoldBeginPerfect = hit.isPerfect;
			note.holdBeginHitTime = hit.time;
		}
	}
	insertion_sort(hits.begin(), hits.end(), [](auto && a, auto && b) {
		return a.time < b.time;
	});
#else
	bernoulli_distribution gHit(gRateHit);
	bernoulli_distribution gHoldBegin(gRateHoldBegin);
	bernoulli_distribution gHoldEnd(gRateHoldEnd);
	bernoulli_distribution gSlide(gRateSlide);
	bernoulli_distribution gSlideHoldEnd(gRateSlideHoldEnd);
	for (auto && hit : hits) {
		if (hit.isSlide) {
			if (hit.isHoldEnd) {
				hit.isPerfect = !gSlideHoldEnd(rng);
			} else {
				hit.isPerfect = !gSlide(rng);
			}
		} else {
			if (hit.isHoldBegin) {
				hit.isPerfect = !gHoldBegin(rng);
			} else if (hit.isHoldEnd) {
				hit.isPerfect = !gHoldEnd(rng);
			} else {
				hit.isPerfect = !gHit(rng);
			}
		}
		if (hit.isHoldBegin) {
			Note & note = notes[hit.noteIndex];
			note.isHoldBeginPerfect = hit.isPerfect;
		}
	}
#endif
}
