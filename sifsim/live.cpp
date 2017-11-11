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


const auto compareTime = [](auto && a, auto && b) {
	return a.time < b.time;
};


bool Live::prepare(const char * json) {
	try {
		rapidjson::Document doc;
		if (doc.Parse(json).HasParseError()) {
			throw runtime_error("Invalid JSON");
		}
		loadSettings(doc);
		loadUnit(doc);
		loadCharts(doc);
	} catch (const runtime_error &) {
		return false;
	}
	return true;
}


void Live::loadSettings(const rapidjson::Value & jsonObj) {
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


void Live::loadUnit(const rapidjson::Value & jsonObj) {
	status = 55820;

	cardNum = 9;
	cards.clear();
	cards.resize(cardNum);

	for (int i = 0; i < cardNum; i++) {
		auto & card = cards[i];
		auto & skill = card.skill;
		card.type = 107;
		card.category = 2;
		card.attribute = 1;
		card.status = 0;
		skill.valid = false;
		card.skillId = (unsigned)i << SKILL_ORDER_SHIFT | (unsigned)i;
	}

	auto & card = cards[0];
	auto & skill = card.skill;
	skill.valid = true;
	skill.effect = Skill::Effect::ScorePlus;
	skill.discharge = Skill::Discharge::Immediate;
	skill.trigger = Skill::Trigger::NotesCount;
	skill.maxLevel = 8;
	skill.levels.resize(skill.maxLevel);
	skill.level = 8;
	auto & level = skill.levels[skill.level - 1];
	level.effectValue = 3500;
	level.dischargeTime = 0;
	level.triggerValue = 22;
	level.activationRate = 50;
	card.skillId |= ActiveSkill;
	card.currentSkillLevel = skill.level;
}


void Live::loadCharts(const rapidjson::Value & jsonObj) {
	memberCategory = 1;
	noteNum = jsonObj.Size();
	notes.resize(noteNum);
	for (int i = 0; i < noteNum; i++) {
		auto && noteObj = GetJsonMemberObject(jsonObj, i);
		auto & note = notes[i];
		int p = GetJsonMemberInt(noteObj, "position");
		note.position = 9 - p;
		note.attribute = GetJsonMemberInt(noteObj, "notes_attribute");
		note.effect((Note::Effect)GetJsonMemberInt(noteObj, "effect"));
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
	if (!is_sorted(notes.begin(), notes.end(), compareTime)) {
		sort(notes.begin(), notes.end(), compareTime);
	}

	processCharts();
}


void Live::processCharts() {
	hits.clear();
	for (int i = 0; i < noteNum; i++) {
		const auto & note = notes[i];
		hits.emplace_back(i, note, false);
		if (note.isHold) {
			hits.emplace_back(i, note, true);
		}
	}
	sort(hits.begin(), hits.end(), compareTime);

	combos.clear();
	combos.reserve(noteNum);
	for (auto && h : hits) {
		if (!h.isHoldBegin) {
			combos.push_back(h.time);
		}
	}
	assert(combos.size() == noteNum);
}


int Live::simulate(int id, uint64_t seed) {
	rng.seed(seed);
	rng.advance(static_cast<uint64_t>(id) << 32);

	initSimulation();
	simulateHitError();

	for (int i = 0; i < cardNum; i++) {
		auto & card = cards[i];
		const auto & skill = card.skill;
		if (!skill.valid) {
			continue;
		}
		skillSetNextTrigger(card);
	}

	for (;;) {
		if (hitIndex < hits.size()
			&& (skillEvents.empty() || !(skillEvents.top().time < hits[hitIndex].time))
			) {
			const auto & hit = hits[hitIndex];
			time = hit.time;
			bool isPerfect = hit.isPerfect || judgeCount;
			auto & note = notes[hit.noteIndex];
			if (hit.isHoldBegin) {
				note.isHoldBeginPerfect = isPerfect;
				++hitIndex;
				continue;
			}

			++combo;
			if (combo > itComboMul->first) {
				++itComboMul;
			}
			score += computeScore(note, isPerfect);

			++hitIndex;
		} else if (!skillEvents.empty()) {
			auto event = skillEvents.top();
			skillEvents.pop();
			time = event.time;
			auto & card = cards[event.id & SkillIndexMask];
			switch (event.id & SkillEventMask) {
			case SkillOn:
				skillTrigger(card);
				break;
			case SkillOff:
				skillOff(card);
				break;
			}
		} else {
			break;
		}
	}

	return static_cast<int>(score);
}


void Live::initSimulation() {
	time = 0;
	hitIndex = 0;
	score = 0;
	combo = 0;
	perfect = 0;
	starPerfect = 0;
	judgeCount = 0;
	itComboMul = COMBO_MUL.cbegin();
	assert(skillEvents.empty());
	assert(scoreTriggers.empty());
	assert(perfectTriggers.empty());
	assert(starPerfectTriggers.empty());
	initSkills();
	if (true) {
		shuffleSkills();
	}
}


void Live::shuffleSkills() {
	for (int i = cardNum; i > 1; --i) {
		swapBits(cards[i - 1].skillId, cards[rng(i)].skillId, (unsigned)SkillOrderMask);
	}
}


void Live::initSkills() {
	for (auto && card : cards) {
		const auto & skill = card.skill;
		if (!skill.valid) {
			continue;
		}
		card.currentSkillLevel = skill.level;
		card.nextTrigger = card.skillLevel().triggerValue;
	}
}


void Live::simulateHitError() {
#if SIMULATE_HIT_TIMING
	normal_distribution<> eHit(0, sigmaHit);
	normal_distribution<> eHoldBegin(0, sigmaHoldBegin);
	normal_distribution<> eHoldEnd(0, sigmaHoldEnd);
	normal_distribution<> eSlide(0, sigmaSlide);
	for (auto && hit : hits) {
		auto & note = notes[hit.noteIndex];
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
	insertion_sort(hits.begin(), hits.end(), compareTime);
	assert(is_sorted(hits.begin(), hits.end(), compareTime));
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
			auto & note = notes[hit.noteIndex];
			note.isHoldBeginPerfect = hit.isPerfect;
		}
	}
#endif
}


double Live::computeScore(const LiveNote & note, bool isPerfect) const {
	const auto & card = cards[note.position];
	double noteScore = status;
	noteScore *= isPerfect ? 1.25 : 1.1;
	noteScore *= itComboMul->second;
	// Doesn't judge accuracy?
	// L7_84 = L12_12.SkillEffect.PerfectBonus.apply(L7_84)
	if (card.category == memberCategory) {
		noteScore *= 1.1;
	}
	if (note.isHold) {
		noteScore *= note.isHoldBeginPerfect ? 1.25 : 1.1;
	}
	if (note.isSlide) {
		noteScore *= 0.5;
	}
	if (card.attribute == note.attribute) {
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


void Live::skillTrigger(LiveCard & card) {
	const auto & level = card.skillLevel();
	if ((int)rng(100) < level.activationRate) {
		skillOn(card);
	}
	skillSetNextTrigger(card);
}


void Live::skillOn(LiveCard & card) {
	const auto & skill = card.skill;
	const auto & level = card.skillLevel();

	score += level.effectValue;

	switch (skill.discharge) {
	case Skill::Discharge::Immediate:
		skillSetNextTrigger(card);
		break;
	case Skill::Discharge::Duration:
		skillEvents.push({ time + level.dischargeTime , SkillOff | card.skillId });
		break;
	}
}


void Live::skillOff(LiveCard & card) {
	skillSetNextTrigger(card);
}


void Live::skillSetNextTrigger(LiveCard & card) {
	const auto & skill = card.skill;
	const auto & level = card.skillLevel();

	switch ((Skill::Trigger)skill.trigger) {
	case Skill::Trigger::NotesCount:
		if (card.nextTrigger <= noteNum) {
			skillEvents.push({ notes[card.nextTrigger - 1].showTime, SkillOn | card.skillId });
			card.nextTrigger += level.triggerValue;
		}
		break;
	}
}
