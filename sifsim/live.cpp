#include "configure.h"
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
	double sigmaHit = 0.015;
	double sigmaHoldBegin = 0.015;
	double sigmaHoldEnd = 0.018;
	double sigmaSlide = 0.030;
#if SIMULATE_HIT_TIMING
	eHit.param(NormalDistribution<>::param_type(0, sigmaHit));
	eHoldBegin.param(NormalDistribution<>::param_type(0, sigmaHoldBegin));
	eHoldEnd.param(NormalDistribution<>::param_type(0, sigmaHoldEnd));
	eSlide.param(NormalDistribution<>::param_type(0, sigmaSlide));
#else
	double gRateHit = erfc(PERFECT_WINDOW / sigmaHit * SQRT1_2);
	double gRateHoldBegin = erfc(PERFECT_WINDOW / sigmaHoldBegin * SQRT1_2);
	double gRateHoldEnd = erfc(PERFECT_WINDOW / sigmaHoldEnd * SQRT1_2);
	double gRateSlide = erfc(GREAT_WINDOW / sigmaSlide * SQRT1_2);
	double gRateSlideHoldEnd = erfc(GREAT_WINDOW / sigmaHoldEnd * SQRT1_2);
	gHit.param(BernoulliDistribution::param_type(gRateHit));
	gHoldBegin.param(BernoulliDistribution::param_type(gRateHoldBegin));
	gHoldEnd.param(BernoulliDistribution::param_type(gRateHoldEnd));
	gSlide.param(BernoulliDistribution::param_type(gRateSlide));
	gSlideHoldEnd.param(BernoulliDistribution::param_type(gRateSlideHoldEnd));
#endif
}


void Live::loadUnit(const rapidjson::Value & jsonObj) {
	status = 80461;
	cardNum = 9;
	cards.clear();
	cards.resize(cardNum);
	for (int i = 0; i < cardNum; i++) {
		auto & card = cards[i];
		auto & skill = card.skill;
		switch (i) {
		case 0:
		//case 8:
		case -1:
		{
			card.type = 105;
			card.category = 2;
			card.attribute = 2;
			card.status = 0;
			skill.valid = true;
			skill.effect = Skill::Effect::Mimic;
			skill.discharge = Skill::Discharge::Immediate;
			skill.trigger = Skill::Trigger::NotesCount;
			card.skillId = ActiveSkill;
			skill.maxLevel = 8;
			skill.levels.resize(skill.maxLevel);
			skill.level = 8;
			skill.chainTypeNum = 0;
			auto & level = skill.levels[skill.level - 1];
			level.effectValue = 0;
			level.dischargeTime = 0;
			level.triggerValue = 20;
			level.activationRate = 43;
			break;
		}
		default:
		{
			card.type = 107;
			card.category = 2;
			card.attribute = 2;
			card.status = 0;
			skill.valid = true;
			skill.effect = Skill::Effect::ScorePlus;
			skill.discharge = Skill::Discharge::Immediate;
			skill.trigger = Skill::Trigger::ComboCount;
			card.skillId = ActiveSkill;
			skill.maxLevel = 8;
			skill.levels.resize(skill.maxLevel);
			skill.level = 8;
			skill.chainTypeNum = 0;
			auto & level = skill.levels[skill.level - 1];
			level.effectValue = 3260 * 2 / 2;
			level.dischargeTime = 0;
			level.triggerValue = 25;
			level.activationRate = 61;
			break;
		}
		}
		card.skillId |= (unsigned)i << SKILL_ORDER_SHIFT | (unsigned)i;
		card.chainStatus.clear();
		card.chainStatus.resize(skill.chainTypeNum);
	}
	/*
	cards[0].skill.trigger = Skill::Trigger::Score;
	auto & level = cards[0].skill.levels[cards[0].skill.level - 1];
	level.effectValue = 1500;
	level.triggerValue = 13500;
	level.activationRate = 20;
	//*/
}


void Live::loadCharts(const rapidjson::Value & jsonObj) {
	chartNum = 1;
	charts.resize(chartNum);
	int totalNotes = 0;
	memberCategory = 1;
	for (int k = 0; k < chartNum; k++) {
		auto & chart = charts[k];
		chart.noteNum = jsonObj.Size();
		chart.beginNote = totalNotes;
		totalNotes += chart.noteNum;
		chart.endNote = totalNotes;
		chart.notes.resize(chart.noteNum);
		for (int i = 0; i < chart.noteNum; i++) {
			auto && noteObj = GetJsonMemberObject(jsonObj, i);
			auto & note = chart.notes[i];
			int p = GetJsonMemberInt(noteObj, "position");
			if (p <= 0 || p > cardNum) {
				throw runtime_error("Invalid note position");
			}
			note.position = cardNum - p;
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
		if (!is_sorted(chart.notes.begin(), chart.notes.end(), compareTime)) {
			sort(chart.notes.begin(), chart.notes.end(), compareTime);
		}
		chart.lastNoteShowTime = chart.notes.empty() ? 0 : chart.notes.back().showTime;
	}

	processCharts();
}


void Live::processCharts() {
	chartHits.resize(chartNum);
	combos.reserve(accumulate(charts.begin(), charts.end(), 0, [](auto && x, auto && c) {
		return x + c.noteNum;
	}));
	for (int k = 0; k < chartNum; k++) {
		auto & chart = charts[k];
		auto & hits = chartHits[k];

		hits.clear();
		for (int i = 0; i < chart.noteNum; i++) {
			const auto & note = chart.notes[i];
			hits.emplace_back(i, note, false);
			if (note.isHold) {
				hits.emplace_back(i, note, true);
			}
		}
		sort(hits.begin(), hits.end(), compareTime);

		for (auto && h : hits) {
			if (!h.isHoldBegin) {
				combos.push_back(h.time);
			}
		}
		assert(combos.size() == chart.endNote);
	}
}


int Live::simulate(int id, uint64_t seed) {
	rng.seed(seed);
	rng.advance(static_cast<uint64_t>(id) << 32);
	initSimulation();
	simulateHitError();
	startSkillTrigger();
	for (chartIndex = 0; chartIndex < chartNum; chartIndex++) {
		if (chartIndex > 0) {
			initNextSong();
		}
		auto & chart = charts[chartIndex];
		auto & hits = chartHits[chartIndex];
		for (;;) {
			if (hitIndex < hits.size()
				&& (skillEvents.empty() || !(skillEvents.top().time < hits[hitIndex].time))
				) {
				const auto & hit = hits[hitIndex];
				time = hit.time;
				bool isPerfect = hit.isPerfect || judgeCount;
				auto & note = chart.notes[hit.noteIndex];
				if (hit.isHoldBegin) {
					note.isHoldBeginPerfect = isPerfect;
					++hitIndex;
					continue;
				}

				++combo;
				if (combo > itComboMul->first) {
					++itComboMul;
				}
				if (isPerfect && (!hit.isHoldEnd || note.isHoldBeginPerfect)) {
					++perfect;
					for (; !perfectTriggers.empty() && perfect >= perfectTriggers.top().value;
						perfectTriggers.pop()
						) {
						skillEvents.emplace(time, perfectTriggers.top().id);
					}
					if (note.isBomb) {
						++starPerfect;
						for (; !starPerfectTriggers.empty()
							&& starPerfect >= starPerfectTriggers.top().value;
							starPerfectTriggers.pop()
							) {
							skillEvents.emplace(time, starPerfectTriggers.top().id);
						}
					}
				}
				score += computeScore(note, isPerfect);
				for (; !scoreTriggers.empty() && score >= scoreTriggers.top().value;
					scoreTriggers.pop()
					) {
					skillEvents.emplace(time, scoreTriggers.top().id);
				}

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
				case SkillNextTrigger:
					card.isActive = false;
					skillSetNextTrigger(card);
					break;
				case SkillOff:
					skillOff(card);
					break;
				}
			} else {
				break;
			}
		}
	}
	clear(scoreTriggers);
	clear(perfectTriggers);
	clear(starPerfectTriggers);
	return static_cast<int>(score);
}


void Live::initSimulation() {
	chartIndex = 0;
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


void Live::initNextSong() {
	time = 0;
	hitIndex = 0;
	assert(!judgeCount);
	initSkillsForNextSong();
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
		card.isActive = false;
		card.nextTrigger = 0;
		card.remainingChainTypeNum = skill.chainTypeNum;
		fill(card.chainStatus.begin(), card.chainStatus.end(), 1);
	}
	initSkillsForEverySong();
}


void Live::initSkillsForNextSong() {
	for (auto && card : cards) {
		const auto & skill = card.skill;
		if (!skill.valid) {
			continue;
		}
		assert(card.currentSkillLevel == skill.level);
		assert(!card.isActive);
		switch (skill.trigger) {
		case Skill::Trigger::Time:
			card.nextTrigger = 0;
			skillSetNextTrigger(card);
			break;
		case Skill::Trigger::NotesCount:
		case Skill::Trigger::ComboCount:
			skillSetNextTrigger(card);
			break;
		case Skill::Trigger::Chain:
			card.remainingChainTypeNum = skill.chainTypeNum;
			fill(card.chainStatus.begin(), card.chainStatus.end(), 1);
			break;
		}
	}
	initSkillsForEverySong();
}


void Live::initSkillsForEverySong() {
	for (auto && card : cards) {
		const auto & skill = card.skill;
		if (!skill.valid) {
			continue;
		}
		if (skill.effect == Skill::Effect::Mimic) {
			card.mimicSkillIndex = -1;
			card.mimicSkillLevel = 0;
		}
	}
	mimicStack.pushTime = -1;
	mimicStack.popTime = 0;
	mimicStack.skillId = 0;
	mimicStack.skillLevel = 0;
}


void Live::simulateHitError() {
#if SIMULATE_HIT_TIMING
	for (int k = 0; k < chartNum; k++) {
		auto & notes = charts[k].notes;
		auto & hits = chartHits[k];
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
#if USE_INSERTION_SORT
		insertionSort(hits.begin(), hits.end(), compareTime);
#else
		sort(hits.begin(), hits.end(), compareTime);
#endif
	}
#else
	for (int k = 0; k < chartNum; k++) {
		auto & notes = charts[k].notes;
		auto & hits = chartHits[k];
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
	}
#endif
}


void Live::startSkillTrigger() {
	for (int i = 0; i < cardNum; i++) {
		auto & card = cards[i];
		const auto & skill = card.skill;
		if (!skill.valid) {
			continue;
		}
		skillSetNextTrigger(card);
	}
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
#if USE_SSE_4_1_ROUND
	// For all IEEE 754 binary64 |x| < 2^53, floor(rn(x / 100)) == floor(rn(x * rn(0.01)))
	//noteScore = FloorSse4_1(noteScore * 0.01);
	noteScore = FloorSse4_1(noteScore / 100.);
#else
	noteScore = floor(noteScore / 100.);
#endif
	if (isPerfect) { // Only judge hold end accuracy?
					 // L7_84 = L7_84 + L12_12.SkillEffect.PerfectBonus.sumBonus(A3_80)
	}
	// L7_84 = L12_12.Combo.applyFixedValueBonus(L7_84)
	// L8_117 = L19_19.SkillEffect.ScoreBonus.apply(L9_118)
	// L9_118 = L7_116 * bonus_score_rate
#if USE_SSE_4_1_ROUND
	return CeilSse4_1(noteScore);
#else
	return ceil(noteScore);
#endif
}


void Live::skillTrigger(LiveCard & card) {
	auto & skill = card.skill;
	assert(skill.valid && !card.isActive);
	bool isMimic = skill.effect == Skill::Effect::Mimic;
	if (isMimic) {
		if (!getMimic(card)) {
			skillSetNextTriggerOnNextFrame(card);
			return;
		}
	}
	const auto & level = card.skillLevel();
	if ((int)rng(100) < level.activationRate) {
		skillOn(card, isMimic);
	} else {
		skillSetNextTriggerOnNextFrame(card);
	}
}


void Live::skillOn(LiveCard & card, bool isMimic) {
	const auto & skill = isMimic ? cards[card.mimicSkillIndex].skill : card.skill;
	const auto & level = isMimic ? skill.levels[card.mimicSkillLevel - 1] : card.skillLevel();

	switch (skill.effect) {
	case Skill::Effect::GreatToPerfect:
	case Skill::Effect::GoodToPerfect:
		if (!judgeCount) {
			// Set judge SIS
		}
		judgeCount++;
		break;
	case Skill::Effect::HpRestore:
		break;
	case Skill::Effect::ScorePlus:
		score += level.effectValue;
		break;
	case Skill::Effect::SkillRateUp:
		break;
	case Skill::Effect::Mimic:
		assert(false);
		break;
	case Skill::Effect::PerfectBonusRatio:
		break;
	case Skill::Effect::PerfectBonusFixedValue:
		break;
	case Skill::Effect::ComboBonusRatio:
		break;
	case Skill::Effect::ComboBonusFixedValue:
		break;
	case Skill::Effect::SyncStatus:
		break;
	case Skill::Effect::GainSkillLevel:
		break;
	case Skill::Effect::GainStatus:
		break;
	}

	switch (skill.discharge) {
	case Skill::Discharge::Immediate:
	default:
		skillSetNextTriggerOnNextFrame(card);
		break;
	case Skill::Discharge::Duration:
		card.isActive = true;
		skillEvents.emplace(time + level.dischargeTime, SkillOff | card.skillId);
		break;
	}

	updateChain(card);
	updateMimic(card);
}


void Live::skillOff(LiveCard & card) {
	assert(card.skill.valid && card.isActive);
	bool isMimic = card.skill.effect == Skill::Effect::Mimic;
	const auto & skill = isMimic ? cards[card.mimicSkillIndex].skill : card.skill;

	switch (skill.effect) {
	case Skill::Effect::GreatToPerfect:
	case Skill::Effect::GoodToPerfect:
		assert(judgeCount);
		--judgeCount;
		if (!judgeCount) {
			// Reset judge SIS
		}
		break;
	case Skill::Effect::SkillRateUp:
		break;
	case Skill::Effect::PerfectBonusRatio:
		break;
	case Skill::Effect::PerfectBonusFixedValue:
		break;
	case Skill::Effect::ComboBonusRatio:
		break;
	case Skill::Effect::ComboBonusFixedValue:
		break;
	case Skill::Effect::SyncStatus:
		break;
	case Skill::Effect::GainSkillLevel:
		break;
	case Skill::Effect::GainStatus:
		break;
	}

	card.isActive = false;
	skillSetNextTrigger(card);
}


void Live::skillSetNextTrigger(LiveCard & card) {
	assert(!card.isActive);
	const auto & skill = card.skill;
	const auto & level = card.skillLevel();
	switch (skill.trigger) {
	case Skill::Trigger::Time:
	{
		const auto & chart = charts[chartIndex];
		double triggerTime = time + level.triggerValue;
		if (!(triggerTime < chart.lastNoteShowTime)) {
			break;
		}
		skillEvents.emplace(triggerTime, SkillOn | card.skillId);
		//card.nextTrigger = triggerTime;
		break;
	}
	case Skill::Trigger::NotesCount:
	{
		const auto & chart = charts[chartIndex];
		int triggerNote = card.nextTrigger + level.triggerValue;
		if (triggerNote > chart.endNote) {
			break;
		}
		double triggerTime = chart.notes[triggerNote - chart.beginNote - 1].showTime;
		if (time < triggerTime) {
			skillEvents.emplace(triggerTime, SkillOn | card.skillId);
		} else {
			skillEvents.emplace(time, SkillOn | card.skillId);
			while (triggerNote + level.triggerValue <= chart.endNote
				&& !(time < chart.notes[triggerNote + level.triggerValue - chart.beginNote - 1].showTime)
				) {
				triggerNote += level.triggerValue;
			}
		}
		card.nextTrigger = triggerNote;
		break;
	}
	case Skill::Trigger::ComboCount:
	{
		const auto & chart = charts[chartIndex];
		int triggerCombo = card.nextTrigger + level.triggerValue;
		if (triggerCombo > chart.endNote) {
			break;
		}
		if (combo < triggerCombo) {
			double triggerTime = combos[triggerCombo - 1];
			skillEvents.emplace(triggerTime, SkillOn | card.skillId);
		} else {
			skillEvents.emplace(time, SkillOn | card.skillId);
			while (!(combo < triggerCombo + level.triggerValue)) {
				triggerCombo += level.triggerValue;
			}
		}
		card.nextTrigger = triggerCombo;
		break;
	}
	case Skill::Trigger::Score:
	{
		int triggerScore = card.nextTrigger + level.triggerValue;
		if (score < triggerScore) {
			scoreTriggers.emplace(triggerScore, SkillOn | card.skillId);
		} else {
			skillEvents.emplace(time, SkillOn | card.skillId);
			while (!(score < triggerScore + level.triggerValue)) {
				triggerScore += level.triggerValue;
			}
		}
		card.nextTrigger = triggerScore;
		break;
	}
	case Skill::Trigger::PerfectCount:
	{
		int triggerPerfect = card.nextTrigger + level.triggerValue;
		if (perfect < triggerPerfect) {
			perfectTriggers.emplace(triggerPerfect, SkillOn | card.skillId);
		} else {
			skillEvents.emplace(time, SkillOn | card.skillId);
			while (!(perfect < triggerPerfect + level.triggerValue)) {
				triggerPerfect += level.triggerValue;
			}
		}
		card.nextTrigger = triggerPerfect;
		break;
	}
	case Skill::Trigger::StarPerfect:
	{
		int triggerStarPerfect = card.nextTrigger + level.triggerValue;
		if (starPerfect < triggerStarPerfect) {
			starPerfectTriggers.emplace(triggerStarPerfect, SkillOn | card.skillId);
		} else {
			skillEvents.emplace(time, SkillOn | card.skillId);
			while (!(starPerfect < triggerStarPerfect + level.triggerValue)) {
				triggerStarPerfect += level.triggerValue;
			}
		}
		card.nextTrigger = triggerStarPerfect;
		break;
	}
	case Skill::Trigger::Chain:
	{
		if (!card.remainingChainTypeNum) {
			skillEvents.emplace(time, SkillOn | card.skillId);
		}
		break;
	}
	}
}


void Live::skillSetNextTriggerOnNextFrame(LiveCard & card) {
	const auto & skill = card.skill;
#if FORCE_SKILL_FRAME_DELAY
	bool needDelay = (skill.trigger != Skill::Trigger::Time);
#elif FORCE_SCORE_TRIGGERED_SKILL_FRAME_DELAY
	bool needDelay = (skill.trigger == Skill::Trigger::Score);
#else
	bool needDelay = false;
#endif
	if (needDelay)
	{
		card.isActive = true;
		skillEvents.emplace(time + FRAME_TIME, SkillNextTrigger | card.skillId);
	} else {
		skillSetNextTrigger(card);
	}
}


void Live::updateChain(const LiveCard & otherCard) {
	const auto & otherSkill = otherCard.skill;
	assert(otherSkill.valid);
	if (otherSkill.trigger == Skill::Trigger::Chain) {
		return;
	}
	for (auto && i : chainTriggers) {
		auto & chainCard = cards[i];
		if (!chainCard.remainingChainTypeNum) {
			continue;
		}
		const auto & chainSkill = chainCard.skill;
		auto type = find_if(chainSkill.chainTargets.begin(), chainSkill.chainTargets.end(),
			[&otherCard](auto && a) {
			return a.first == otherCard.type;
		});
		if (type == chainSkill.chainTargets.end()) {
			continue;
		}
		const auto & index = type->second;
		chainCard.remainingChainTypeNum -= chainCard.chainStatus[index];
		chainCard.chainStatus[index] = 0;
		if (!chainCard.remainingChainTypeNum && !chainCard.isActive) {
			skillEvents.emplace(time, SkillOn | chainCard.skillId);
		}
	}
}


void Live::updateMimic(const LiveCard & otherCard) {
	const auto & otherSkill = otherCard.skill;
	assert(otherSkill.valid);
	if (otherSkill.effect == Skill::Effect::Mimic) {
		return;
	}
	if (time > mimicStack.pushTime) {
		mimicStack.pushTime = time;
		mimicStack.skillId = otherCard.skillId;
		mimicStack.skillLevel = otherCard.currentSkillLevel;
	}
}


bool Live::getMimic(LiveCard & card) {
	assert(card.skill.valid && card.skill.effect == Skill::Effect::Mimic);
	assert(time >= mimicStack.pushTime && time >= mimicStack.popTime);
	if (mimicStack.popTime > mimicStack.pushTime) {
		card.mimicSkillIndex = -1;
		card.mimicSkillLevel = 0;
		return false;
	}
	mimicStack.popTime = time;
	card.mimicSkillIndex = mimicStack.skillId & SkillIndexMask;
	card.mimicSkillLevel = mimicStack.skillLevel;
	return true;
}
