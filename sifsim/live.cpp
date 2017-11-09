#include "live.h"
#include "util.h"
#include "rapidjson/document.h"
#include "rapidjsonutil.h"
#include <cmath>
#include <queue>
#include <stdexcept>
#include <algorithm>
#include <numeric>

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
	constexpr double SQRT1_2 = 0.707106781186547524401;
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
	strength = 55820;
	attributes.clear();
	attributes.resize(9, 1);
	skills.clear();

	Skill skill;
	skill.effect = (int)Skill::Effect::ScorePlus;
	skill.discharge = (int)Skill::Discharge::Immediate;
	skill.trigger = (int)Skill::Trigger::NotesCount;
	skill.level = 8;
	skill.maxLevel = 8;
	skill.levelData.effectValue = 3500;
	skill.levelData.dischargeTime = 0;
	skill.levelData.triggerValue = 22;
	skill.levelData.activationRate = 50;
	skills.push_back(skill);
	skillNum = static_cast<int>(skills.size());

	skillIds.clear();
	skillIds.resize(skillNum);
	for (int i = 0; i < skillNum; i++) {
		skillIds[i] = (unsigned)i << 16 | (unsigned)i;
	}
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

	int trigger = 0;
	for (int i = 0; i < skillNum; i++) {
		const Skill & skill = skills[i];
		switch ((Skill::Trigger)skill.trigger) {
		case Skill::Trigger::NotesCount:
			trigger += skill.levelData.triggerValue;
			timedEvents.push({
				notes[trigger - 1].showTime,
				TimedEvent::Type::ActiveSkillOn,
				0
			});
			break;
		}
	}

	for (;;) {
		if (hitIndex < hits.size() && (timedEvents.empty()
			|| !(timedEvents.top().time < hits[hitIndex].time))) {

			const Hit & hit = hits[hitIndex];
			if (hit.isHoldBegin) {
				bool isPerfect = hit.isPerfect || judgeCount;
				Note & note = notes[hit.noteIndex];
				note.isHoldBeginPerfect = isPerfect;
				++hitIndex;
				continue;
			}

			++combo;
			if (combo > itComboMul->first) {
				++itComboMul;
			}
			score += computeScore();

			++hitIndex;
		} else if (!timedEvents.empty()) {
			TimedEvent event = timedEvents.top();
			const Skill & skill = skills[event.id & 0xffff];
			timedEvents.pop();
			if (rng(100) < skill.levelData.activationRate) {
				score += skill.levelData.effectValue;
			}
			trigger += skill.levelData.triggerValue;
			if (trigger <= noteNum) {
				timedEvents.push({
					notes[trigger - 1].showTime,
					TimedEvent::Type::ActiveSkillOn,
					0
				});
			}
		} else {
			break;
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
	if (!timedEvents.empty()) {
		timedEvents = MinPriorityQueue<TimedEvent>();
	}
	//shuffle(skillIdMap.begin(), skillIdMap.end(), rng);
	//if (skillNum > 0) {
	//	for (int i = skillNum - 1; i > 0; --i) {
	//		swap(skillIdMap[i], skillIdMap[rng(i + 1)]);
	//	}
	//}
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


double Live::computeScore() {
	const Hit & hit = hits[hitIndex];
	bool isPerfect = hit.isPerfect || judgeCount;
	const Note & note = notes[hit.noteIndex];
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
	return ceil(noteScore);
}
