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
#include <type_traits>
#include <cassert>

using namespace std;
using namespace std::literals;


const auto compareTime = [](const auto & a, const auto & b) {
	return a.time < b.time;
};


Live::Live(FILE * fp) {
	rapidjson::Document doc = ParseJsonFile(fp);
	if (!doc.IsObject()) {
		throw JsonParseError("Invalid input");
	}
	loadSettings(GetJsonMember(doc, "settings"));
	auto itLiveBonus = doc.FindMember("live_bonus");
	if (itLiveBonus != doc.MemberEnd() && !itLiveBonus->value.IsNull()) {
		loadLiveBonus(itLiveBonus->value);
	}
	auto itSkillOrder = doc.FindMember("skill_trigger_priority");
	if (itSkillOrder != doc.MemberEnd() && !itSkillOrder->value.IsNull()) {
		loadSkillOrder(itSkillOrder->value);
	}
	loadUnit(GetJsonMember(doc, "cards"));
	loadCharts(GetJsonMember(doc, "lives"));
	processUnit();
	processCharts();
}


void Live::loadSettings(const rapidjson::Value & json) {
	if (!json.IsObject()) {
		throw JsonParseError("Invalid input: settings");
	}
	mode = static_cast<LiveMode>(GetJsonMemberInt(json, "mode"));
	hiSpeed = GetJsonMemberDouble(json, "note_speed");
	judgeOffset = TryGetJsonMemberDouble(json, "judge_offset").value_or(0);
#if SIMULATE_HIT_TIMING
	loadHitError(GetJsonMember(json, "hit_error"));
#else
	auto itJudgeWindow = json.FindMember("judge_window");
	if (itJudgeWindow != json.MemberEnd() && !itJudgeWindow->value.IsNull()) {
		auto & jsonJudgeWindow = itJudgeWindow->value;
		if (!jsonJudgeWindow.IsArray() || jsonJudgeWindow.Size() < 3) {
			throw JsonParseError("Invalid input: settings.judge_window");
		}
		hitPerfectWindow = GetJsonItemDouble(jsonJudgeWindow, 0);
		hitGreatWindow = GetJsonItemDouble(jsonJudgeWindow, 1);
		slidePerfectWindow = hitGreatWindow;
		slideGreatWindow = GetJsonItemDouble(jsonJudgeWindow, 2);
	} else {
		constexpr double JUDGE_MIN_SPEED = 0.8;
		constexpr double PLAYAREA_R = 400;
		constexpr double PERFECT_WINDOW_TICKS = 16;
		constexpr double GREAT_WINDOW_TICKS = 40;
		constexpr double GOOD_WINDOW_TICKS = 64;
		double judgeTick = fmax(hiSpeed, JUDGE_MIN_SPEED) / PLAYAREA_R;
		hitPerfectWindow = judgeTick * PERFECT_WINDOW_TICKS;
		hitGreatWindow = judgeTick * GREAT_WINDOW_TICKS;
		slidePerfectWindow = hitGreatWindow;
		slideGreatWindow = judgeTick * GOOD_WINDOW_TICKS;
	}
	auto itHitError = json.FindMember("hit_error");
	if (itHitError != json.MemberEnd() && !itHitError->value.IsNull()) {
		loadHitError(itHitError->value);
	} else {
		loadGreatRate(GetJsonMember(json, "hit_great_rate"));
	}
#endif
}


#if SIMULATE_HIT_TIMING

void Live::loadHitError(const rapidjson::Value & json) {
	if (!json.IsObject()) {
		throw JsonParseError("Invalid input: settings.hit_error");
	}
	const auto readParam = [](const rapidjson::Value & obj) {
		double mean = TryGetJsonMemberDouble(obj, "mean").value_or(0);
		double stddev = GetJsonMemberDouble(obj, "stddev");
		return NormalDistribution<>::param_type(mean, stddev);
	};
	eHit.param(readParam(GetJsonMemberObject(json, "hit")));
	eHoldBegin.param(readParam(GetJsonMemberObject(json, "hold_begin")));
	eHoldEnd.param(readParam(GetJsonMemberObject(json, "hold_end")));
	eSlide.param(readParam(GetJsonMemberObject(json, "slide")));
}

#else

void Live::loadHitError(const rapidjson::Value & json) {
	if (!json.IsObject()) {
		throw JsonParseError("Invalid input: settings.hit_error");
	}
	const auto readParam = [](const rapidjson::Value & obj, double window) {
		constexpr double SQRT1_2 = 0.707106781186547524401;
		double mean = TryGetJsonMemberDouble(obj, "mean").value_or(0);
		double stddev = GetJsonMemberDouble(obj, "stddev");
		double gRate = 0.5 * (erfc((window - mean) / stddev * SQRT1_2)
			+ erfc((window + mean) / stddev * SQRT1_2));
		return BernoulliDistribution::param_type(gRate);
	};
	gHit.param(readParam(GetJsonMemberObject(json, "hit"), hitPerfectWindow));
	gHoldBegin.param(readParam(GetJsonMemberObject(json, "hold_begin"), hitPerfectWindow));
	gHoldEnd.param(readParam(GetJsonMemberObject(json, "hold_end"), hitPerfectWindow));
	gSlide.param(readParam(GetJsonMemberObject(json, "slide"), slidePerfectWindow));
	gSlideHoldEnd.param(readParam(GetJsonMemberObject(json, "hold_end"), slidePerfectWindow));
}


void Live::loadGreatRate(const rapidjson::Value & json) {
	if (!json.IsObject()) {
		throw JsonParseError("Invalid input: settings.hit_great_rate");
	}
	gHit.param(BernoulliDistribution::param_type(GetJsonMemberDouble(json, "hit")));
	gHoldBegin.param(BernoulliDistribution::param_type(GetJsonMemberDouble(json, "hold_begin")));
	gHoldEnd.param(BernoulliDistribution::param_type(GetJsonMemberDouble(json, "hold_end")));
	gSlide.param(BernoulliDistribution::param_type(GetJsonMemberDouble(json, "slide")));
	gSlideHoldEnd.param(BernoulliDistribution::param_type(GetJsonMemberDouble(json, "hold_end")));
}

#endif


void Live::loadLiveBonus(const rapidjson::Value & json) {
	if (!json.IsObject()) {
		throw JsonParseError("Invalid input: live_bonus");
	}
	liveScoreRate = TryGetJsonMemberDouble(json, "bonus_score_rate").value_or(1);
	liveActivationRate = TryGetJsonMemberDouble(json, "bonus_activation_rate").value_or(1);
	auto itGuestBonus = json.FindMember("guest_bonus");
	if (itGuestBonus != json.MemberEnd() && !itGuestBonus->value.IsNull()) {
		// TODO: load MF guest bonus
		throw runtime_error("MF guest bonus not implemented");
	}
}


void Live::loadSkillOrder(const rapidjson::Value & json) {
	if (!json.IsArray()) {
		throw JsonParseError("Invalid input: skill_trigger_priority");
	}
	for (const auto & i : json.GetArray()) {
		if (!i.IsInt()) {
			throw JsonParseError("Invalid input: skill_trigger_priority");
		}
		skillOrder.emplace_back(i.GetInt());
	}
}


void Live::loadUnit(const rapidjson::Value & json) {
	if (!json.IsArray()) {
		throw JsonParseError("Invalid input: cards");
	}
	unitStatus = 0;
	judgeSisStatus = 0;
	cards.reserve(json.Size());
	for (const auto & jsonCard : json.GetArray()) {
		if (!jsonCard.IsObject()) {
			throw JsonParseError("Invalid input: cards");
		}
		cards.emplace_back();
		auto & card = cards.back();
		card.type = GetJsonMemberInt(jsonCard, "unit_type");
		card.category = GetJsonMemberInt(jsonCard, "member_category");
		card.attribute = GetJsonMemberInt(jsonCard, "attribute");
		card.baseStatus = GetJsonMemberInt(jsonCard, "base_status");
		card.status = GetJsonMemberInt(jsonCard, "status");
		unitStatus += card.status;

		auto & skill = card.skill;
		auto itSkill = jsonCard.FindMember("skill");
		if (itSkill == jsonCard.MemberEnd() || itSkill->value.IsNull()) {
			skill.effect = Skill::Effect::None;
		} else {
			const auto & jsonSkill = itSkill->value;
			if (!jsonSkill.IsObject()) {
				throw JsonParseError("Invalid input: cards[].skill");
			}
			skill.effect = static_cast<Skill::Effect>(GetJsonMemberInt(jsonSkill, "effect_type"));
			skill.discharge = static_cast<Skill::Discharge>(GetJsonMemberInt(jsonSkill, "discharge_type"));
			skill.trigger = static_cast<Skill::Trigger>(GetJsonMemberInt(jsonSkill, "trigger_type"));
			skill.level = GetJsonMemberInt(jsonSkill, "level");
			auto itEffectTargets = jsonSkill.FindMember("effect_targets");
			if (itEffectTargets != jsonSkill.MemberEnd() && !itEffectTargets->value.IsNull()) {
				const auto & jsonEffectTargets = itEffectTargets->value;
				if (!jsonEffectTargets.IsArray()) {
					throw JsonParseError("Invalid input: cards[].skill.effect_targets");
				}
				for (const auto & target : jsonEffectTargets.GetArray()) {
					if (!target.IsInt()) {
						throw JsonParseError("Invalid input: cards[].skill.effect_targets");
					}
					skill.effectTargets.emplace_back(target.GetInt());
				}
			}
			auto itTriggerTargets = jsonSkill.FindMember("trigger_targets");
			if (itTriggerTargets != jsonSkill.MemberEnd() && !itTriggerTargets->value.IsNull()) {
				const auto & jsonTriggerTargets = itTriggerTargets->value;
				if (!jsonTriggerTargets.IsArray()) {
					throw JsonParseError("Invalid input: cards[].skill.trigger_targets");
				}
				for (const auto & target : jsonTriggerTargets.GetArray()) {
					if (!target.IsInt()) {
						throw JsonParseError("Invalid input: cards[].skill.trigger_targets");
					}
					skill.chainTargets.emplace_back(target.GetInt());
				}
			}
			const auto & jsonLevels = GetJsonMemberArray(jsonSkill, "levels");
			for (const auto & jsonLevel : jsonLevels.GetArray()) {
				if (!jsonLevel.IsObject()) {
					throw JsonParseError("Invalid input: cards[].skill.levels");
				}
				skill.levels.emplace_back();
				auto & level = skill.levels.back();
				level.effectValue = GetJsonMemberDouble(jsonLevel, "effect_value");
				level.dischargeTime = GetJsonMemberDouble(jsonLevel, "discharge_time");
				level.triggerValue = GetJsonMemberInt(jsonLevel, "trigger_value");
				level.activationRate = GetJsonMemberInt(jsonLevel, "activation_rate");
			}
			auto itSisList = jsonCard.FindMember("school_idol_skills");
			if (itSisList != jsonCard.MemberEnd() && !itSisList->value.IsNull()) {
				// Translate SIS effect, assume FC
				const auto & jsonSisList = itSisList->value;
				if (!jsonSisList.IsObject()) {
					throw JsonParseError("Invalid input: cards[].skill.school_idol_skills");
				}
				auto charm = TryGetJsonMemberDouble(jsonSisList, "charm");
				if (charm && skill.effect == Skill::Effect::ScorePlus) {
					for (auto & level : skill.levels) {
						level.effectValue += Ceil(level.effectValue * *charm / 100.0);
					}
				}
				auto trick = TryGetJsonMemberDouble(jsonSisList, "trick");
				if (trick) {
					judgeSisStatus += Ceil(card.baseStatus * *trick / 100.0);
				}
				auto heal = TryGetJsonMemberDouble(jsonSisList, "heal");
				if (heal && skill.effect == Skill::Effect::HpRestore) {
					skill.effect = Skill::Effect::ScorePlus;
					for (auto & level : skill.levels) {
						level.effectValue *= *heal;
					}
				}
			}
		}
	}
}


void Live::processUnit() {
	// Check skill order
	if (!skillOrder.empty()) {
		if (skillOrder.size() != cards.size()) {
			throw runtime_error("Invalid skill trigger order");
		}
		vector<unsigned char> check(skillOrder.size(), false);
		for (auto i : skillOrder) {
			if (i < 0 || i >= check.size() || exchange(check[i], true)) {
				throw runtime_error("Invalid skill trigger order");
			}
		}
	}

	unsigned i = 0;
	for (auto & card : cards) {
		auto & skill = card.skill;
		// Skill order
		if (skillOrder.empty()) {
			card.skillId = i << SKILL_ORDER_SHIFT | i;
		} else {
			card.skillId = static_cast<unsigned>(skillOrder[i]) << SKILL_ORDER_SHIFT | i;
		}
		i++;
		// Init chain status array
		if (skill.trigger == Skill::Trigger::Chain) {
			card.chainStatus.resize(skill.chainTargets.size());
		}
		// Ignore nop skills (assume FC)
		switch (skill.effect) {
		case Skill::Effect::GreatToPerfect:
		case Skill::Effect::GoodToPerfect:
		case Skill::Effect::ScorePlus:
		case Skill::Effect::SkillRateUp:
		case Skill::Effect::Mimic:
		case Skill::Effect::PerfectBonusRatio:
		case Skill::Effect::PerfectBonusFixedValue:
		case Skill::Effect::SyncStatus:
		case Skill::Effect::GainStatus:
			skill.valid = true;
			break;

		case Skill::Effect::None:
		case Skill::Effect::HpRestore:
			skill.valid = false;
			break;

		case Skill::Effect::GainSkillLevel:
			throw runtime_error("Skill level boost not implemented");

		case Skill::Effect::ComboBonusRatio:
		case Skill::Effect::ComboBonusFixedValue:
			throw runtime_error("Combo fever not implemented");

		default:
			throw runtime_error("Unknown skill effect type: "
				+ to_string(static_cast<underlying_type_t<Skill::Effect>>(skill.effect)));
		}

		for (auto target : skill.effectTargets) {
			if (target < 0 || target >= cards.size()) {
				throw JsonParseError("Invalid skill target list");
			}
		}
	}
}


void Live::loadCharts(const rapidjson::Value & json) {
	if (!json.IsArray()) {
		throw JsonParseError("Invalid input: lives");
	}
	charts.reserve(json.Size());
	int totalNotes = 0;
	for (const auto & jsonChart : json.GetArray()) {
		if (!jsonChart.IsObject()) {
			throw JsonParseError("Invalid input: lives");
		}
		auto path = TryGetJsonMemberString(jsonChart, "livejson_path");
		optional<rapidjson::Document> livejson;
		if (path) {
			livejson = ParseJsonFile(CFileWrapper(*path, "rb"));
			if (!livejson->IsArray()) {
				throw JsonParseError("Invalid livejson file: "s + *path);
			}
		}
		const auto & jsonNotes = path ? *livejson : GetJsonMemberArray(jsonChart, "livejson");
		charts.emplace_back();
		auto & chart = charts.back();

		chart.memberCategory = GetJsonMemberInt(jsonChart, "member_category");
		int noteNum = jsonNotes.Size();
		chart.beginNote = totalNotes;
		totalNotes += noteNum;
		chart.endNote = totalNotes;
		chart.notes.reserve(noteNum);
		for (const auto & noteObj : jsonNotes.GetArray()) {
			if (!noteObj.IsObject()) {
				throw JsonParseError("Invalid livejson");
			}
			chart.notes.emplace_back();
			auto & note = chart.notes.back();

			note.position = GetJsonMemberInt(noteObj, "position");
			note.attribute = GetJsonMemberInt(noteObj, "notes_attribute");
			note.effect(static_cast<Note::Effect>(GetJsonMemberInt(noteObj, "effect")));
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
}


void Live::processCharts() {
	int cardNum = static_cast<int>(cards.size());
	chartHits.reserve(charts.size());
	combos.reserve(accumulate(charts.begin(), charts.end(), size_t{ 0 }, [](size_t x, const auto & c) {
		return x + c.notes.size();
	}));
	for (auto & chart : charts) {
		chartHits.emplace_back();
		auto & hits = chartHits.back();

		// In livejson, leftmost = 9, rightmost = 1
		// Transform to leftmost = 0, rightmost = 8
		for (auto & note : chart.notes) {
			if (note.position <= 0 || note.position > cardNum) {
				throw runtime_error("Invalid note position: " + to_string(note.position));
			}
			note.position = cardNum - note.position;
		}

		for (size_t i = 0; i < chart.notes.size(); i++) {
			const auto & note = chart.notes[i];
			hits.emplace_back(static_cast<int>(i), note, false);
			if (note.isHold) {
				hits.emplace_back(static_cast<int>(i), note, true);
			}
		}
		sort(hits.begin(), hits.end(), compareTime);

		for (const auto & h : hits) {
			if (!h.isHoldBegin) {
				combos.emplace_back(h.time);
			}
		}
		assert(combos.size() == chart.endNote);
	}
}


int Live::simulate(uint64_t id, uint64_t seed) {
	constexpr uint64_t RNG_ADVANCE = UINT64_C(7640891576956012744);
	rng.seed(seed);
	rng.advance(id * RNG_ADVANCE);
	initSimulation();
	simulateHitError();
	startSkillTrigger();
	for (chartIndex = 0; chartIndex < charts.size(); chartIndex++) {
		if (chartIndex > 0) {
			initNextSong();
		}
		auto & chart = charts[chartIndex];
		auto & hits = chartHits[chartIndex];
		for (;;) {
			if (hitIndex < hits.size()
				&& (skillEvents.empty() || !(skillEvents.top().time < hits[hitIndex].time))
				) {
				// Note hit/release
				const auto & hit = hits[hitIndex];
				time = hit.time;
				bool isPerfect = hit.isPerfect || judgeCount;
				auto & note = chart.notes[hit.noteIndex];
				if (hit.isHoldBegin) {
					note.isHoldBeginPerfect = isPerfect;
					++hitIndex;
					continue;
				}
				// Note stats
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
				// Score
				score += computeScore(note, isPerfect);
				for (; !scoreTriggers.empty() && score >= scoreTriggers.top().value;
					scoreTriggers.pop()
					) {
					skillEvents.emplace(time, scoreTriggers.top().id);
				}

				++hitIndex;
			} else if (!skillEvents.empty()) {
				// Skill event
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
	score = 0;
	combo = 0;
	perfect = 0;
	starPerfect = 0;
	itComboMul = COMBO_MUL.cbegin();
	for (auto & card : cards) {
		card.buffedStatus = nullopt;
		card.syncStatus = nullopt;
	}
	initForEverySong();
	assert(skillEvents.empty());
	assert(scoreTriggers.empty());
	assert(perfectTriggers.empty());
	assert(starPerfectTriggers.empty());
	initSkills();
	if (skillOrder.empty()) {
		shuffleSkills();
	}
}


void Live::initNextSong() {
	initForEverySong();
	initSkillsForNextSong();
}


void Live::initForEverySong() {
	assert(activationMod == chartActivationRate);
	assert(!judgeCount);
	const auto & chart = charts[chartIndex];
	status = unitStatus;
	time = 0;
	hitIndex = 0;
	chartMemberCategory = chart.memberCategory;
	chartScoreRate = liveScoreRate;
	chartActivationRate = liveActivationRate;
	activationMod = chartActivationRate;
}


void Live::shuffleSkills() {
	for (uint32_t i = static_cast<uint32_t>(cards.size()); i > 1; --i) {
		swapBits(cards[i - 1].skillId, cards[rng(i)].skillId, SkillOrderMask);
	}
}


void Live::initSkills() {
	for (auto & card : cards) {
		const auto & skill = card.skill;
		if (!skill.valid) {
			continue;
		}
		card.currentSkillLevel = skill.level;
		card.isActive = false;
		card.nextTrigger = 0;
		card.remainingChainTypeNum = static_cast<int>(card.chainStatus.size());
		fill(card.chainStatus.begin(), card.chainStatus.end(), 1);
	}
	initSkillsForEverySong();
}


void Live::initSkillsForNextSong() {
	for (auto & card : cards) {
		const auto & skill = card.skill;
		if (!skill.valid) {
			continue;
		}
		assert(card.currentSkillLevel == skill.level);
		assert(!card.isActive);
		switch (skill.trigger) {
		case Skill::Trigger::None:
		case Skill::Trigger::Score:
		case Skill::Trigger::PerfectCount:
		case Skill::Trigger::StarPerfect:
			break;

		case Skill::Trigger::Time:
			card.nextTrigger = 0;
			skillSetNextTrigger(card);
			break;

		case Skill::Trigger::NotesCount:
		case Skill::Trigger::ComboCount:
			skillSetNextTrigger(card);
			break;

		case Skill::Trigger::Chain:
			// Not cleared between songs?
			/*
			card.remainingChainTypeNum = skill.chainTypeNum;
			fill(card.chainStatus.begin(), card.chainStatus.end(), 1);
			//*/
			break;
		}
	}
	initSkillsForEverySong();
}


void Live::initSkillsForEverySong() {
	for (auto & card : cards) {
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
	for (size_t k = 0; k < charts.size(); k++) {
		auto & notes = charts[k].notes;
		auto & hits = chartHits[k];
		for (auto & hit : hits) {
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
			double greatWindow = hit.isSlide ? slideGreatWindow : hitGreatWindow;
			if (!(fabs(e) < greatWindow)) {
				e = copysign(greatWindow, e);
			}
			if (hit.isHoldEnd) {
				double minTime = note.holdBeginHitTime + FRAME_TIME;
				double minE = minTime - judgeTime;
				if (e < minE) {
					e = minE;
				}
			}
			hit.time = judgeTime + e;
			double perfectWindow = hit.isSlide ? slidePerfectWindow : hitPerfectWindow;
			hit.isPerfect = (fabs(e) < perfectWindow);
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
	for (size_t k = 0; k < charts.size(); k++) {
		auto & notes = charts[k].notes;
		auto & hits = chartHits[k];
		for (auto & hit : hits) {
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
	for (auto & card : cards) {
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
	// TODO: combo fever
	noteScore *= itComboMul->second;
	// Doesn't judge accuracy?
	noteScore *= perfectBonusRate;
	if (card.category == chartMemberCategory) {
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
	noteScore = Floor(noteScore / 100.);
	// Only judge hold end accuracy?
	if (isPerfect) {
		noteScore += perfectBonusFixed;
	}
	// TODO: combo fever
	// L7_84 = L12_12.Combo.applyFixedValueBonus(L7_84)
	// L8_117 = L19_19.SkillEffect.ScoreBonus.apply(L9_118)
	noteScore *= chartScoreRate;
	return Ceil(noteScore);
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
	// Effectively ceil(rate * mod)
	if (rng(100) < level.activationRate * activationMod) {
		skillOn(card, isMimic);
	} else {
		skillSetNextTriggerOnNextFrame(card);
	}
}


void Live::skillOn(LiveCard & card, bool isMimic) {
	const auto & skill = isMimic ? cards[card.mimicSkillIndex].skill : card.skill;
	const auto & level = isMimic ? skill.levels[card.mimicSkillLevel - 1] : card.skillLevel();

	switch (skill.effect) {
	case Skill::Effect::None:
		break;

	case Skill::Effect::GreatToPerfect:
	case Skill::Effect::GoodToPerfect:
		if (!judgeCount) {
			// Technically not the same as SIF (i.e. floating point addition not associative)
			status += judgeSisStatus;
		}
		judgeCount++;
		break;

	case Skill::Effect::HpRestore:
		assert(false);
		throw logic_error("Bug detected. Please contact software developer.\n  (" FILE_LOC ")");
		break;

	case Skill::Effect::ScorePlus:
		score += level.effectValue;
		break;

	case Skill::Effect::SkillRateUp:
		activationMod *= level.effectValue;
		break;

	case Skill::Effect::Mimic:
		assert(false);
		throw logic_error("Bug detected. Please contact software developer.\n  (" FILE_LOC ")");
		break;

	case Skill::Effect::PerfectBonusRatio:
		perfectBonusRateQueue.emplace_back(level.effectValue);
		perfectBonusRate = accumulate(perfectBonusRateQueue.begin(), perfectBonusRateQueue.end(), 1.0);
		break;

	case Skill::Effect::PerfectBonusFixedValue:
		perfectBonusFixedQueue.emplace_back(level.effectValue);
		perfectBonusFixed = accumulate(perfectBonusFixedQueue.begin(), perfectBonusFixedQueue.end(), 0.0);
		break;

	case Skill::Effect::ComboBonusRatio:
		// TODO: combo fever on
		break;

	case Skill::Effect::ComboBonusFixedValue:
		// TODO: combo fever on
		break;

	case Skill::Effect::SyncStatus:
	{
		if (skill.effectTargets.empty()) {
			break;
		}
		auto index = rng(static_cast<uint32_t>(skill.effectTargets.size()));
		const auto & target = cards[skill.effectTargets[index]];
		card.syncStatus = target.getSyncStatus();
		status += *card.syncStatus - card.status;
	}
		break;

	case Skill::Effect::GainSkillLevel:
		// TODO: gain skill level on
		break;

	case Skill::Effect::GainStatus:
		for (const auto & i : skill.effectTargets) {
			auto & target = cards[i];
			// Not the same as SIF (return L5_80.buff_rate > 1)
			if (target.buffedStatus) {
				continue;
			}
			target.buffedStatus = target.status * level.effectValue;
			status += *target.buffedStatus - target.status;
		}
		break;
	}

	switch (skill.discharge) {

	case Skill::Discharge::Immediate:
		skillSetNextTriggerOnNextFrame(card);
		break;

	case Skill::Discharge::Duration:
		card.isActive = true;
		skillEvents.emplace(time + level.dischargeTime, SkillOff | card.skillId);
		break;

	default:
		assert(false);
		throw runtime_error("Invalid skill discharge type");
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
	case Skill::Effect::None:
		break;

	case Skill::Effect::GreatToPerfect:
	case Skill::Effect::GoodToPerfect:
		assert(judgeCount);
		--judgeCount;
		if (!judgeCount) {
			// Technically not the same as SIF (i.e. floating point addition not associative)
			status -= judgeSisStatus;
		}
		break;

	case Skill::Effect::HpRestore:
		break;

	case Skill::Effect::ScorePlus:
		break;

	case Skill::Effect::SkillRateUp:
		// Klab bug? Reset all buff
		activationMod = chartActivationRate;
		break;

	case Skill::Effect::Mimic:
		assert(false);
		throw logic_error("Bug detected. Please contact software developer.\n  (" FILE_LOC ")");
		break;

	case Skill::Effect::PerfectBonusRatio:
		// Klab bug? Out of order
		perfectBonusRateQueue.pop_front();
		perfectBonusRate = accumulate(perfectBonusRateQueue.begin(), perfectBonusRateQueue.end(), 1.0);
		break;

	case Skill::Effect::PerfectBonusFixedValue:
		// Klab bug? Out of order
		perfectBonusFixedQueue.pop_front();
		perfectBonusFixed = accumulate(perfectBonusFixedQueue.begin(), perfectBonusFixedQueue.end(), 0.0);
		break;

	case Skill::Effect::ComboBonusRatio:
		// TODO: combo fever off
		break;

	case Skill::Effect::ComboBonusFixedValue:
		// TODO: combo fever off
		break;

	case Skill::Effect::SyncStatus:
		status -= *card.syncStatus - card.status;
		card.syncStatus = nullopt;
		break;

	case Skill::Effect::GainSkillLevel:
		// TODO: gain skill level off
		break;

	case Skill::Effect::GainStatus:
		for (const auto & i : skill.effectTargets) {
			auto & target = cards[i];
			// Not the same as SIF (return L5_80.buff_rate > 1)
			if (!target.buffedStatus) {
				continue;
			}
			status -= *target.buffedStatus - target.status;
			target.buffedStatus = nullopt;
		}

		break;
	}

	card.isActive = false;
	skillSetNextTrigger(card);
}


void Live::skillSetNextTrigger(LiveCard & card) {
	assert(!card.isActive);
	const auto & skill = card.skill;
	const auto & level = card.skillLevel();

	const auto setTransformedTrigger = [&](
		auto & queue, const auto & curr, bool isCurrTransformed,
		const auto & transform, const auto & pastEnd
		) {
		static_assert(is_same<decay_t<decltype(pastEnd(0))>, bool>::value, "pastEnd should return bool");
		const auto satisfied = [&](auto trigger) {
			if (isCurrTransformed) {
				return !pastEnd(trigger) && curr >= transform(trigger);
			} else {
				return curr >= trigger;
			}
		};
		// old nextTrigger: trigger value of last skill trigger event
		int prev = card.nextTrigger;
		int next = prev + level.triggerValue;
		if (pastEnd(next)) {
			// No skill trigger event queued, don't update nextTrigger
			return;
		}
		if (!satisfied(next)) {
			// Trigger condition not satisfied, queue future trigger event
			queue.emplace(transform(next), SkillOn | card.skillId);
		} else {
			// Trigger condition satisfied, queue current self-overlapping trigger event
			skillEvents.emplace(time, SkillOn | card.skillId);
			// First order self-overlapping only, no high order self-overlapping
			while (satisfied(next + level.triggerValue)) {
				next += level.triggerValue;
			}
		}
		// new nextTrigger: trigger value of skill trigger event in queue
		card.nextTrigger = next;
	};

	const auto setTrigger = [&](auto & queue, const auto & curr) {
		const auto falseFunc = [](int) { return false; };
		const auto identity = [](int trigger) { return trigger; };
		return setTransformedTrigger(queue, curr, false, identity, falseFunc);
	};

	switch (skill.trigger) {
	case Skill::Trigger::None:
		break;

	case Skill::Trigger::Time:
	{
		const auto & chart = charts[chartIndex];
		double triggerTime = time + level.triggerValue;
		if (!(triggerTime < chart.lastNoteShowTime)) {
			break;
		}
		skillEvents.emplace(triggerTime, SkillOn | card.skillId);
		// card.nextTrigger isn't used for time trigger
		break;
	}
	case Skill::Trigger::NotesCount:
	{
		const auto & chart = charts[chartIndex];
		const auto getTime = [&](int note) { return chart.notes[note - chart.beginNote - 1].showTime; };
		const auto pastEnd = [&](int note) { return note > chart.endNote; };
		setTransformedTrigger(skillEvents, time, true, getTime, pastEnd);
		break;
	}
	case Skill::Trigger::ComboCount:
	{
		const auto & chart = charts[chartIndex];
		const auto getTime = [&](int combo) { return combos[combo - 1]; };
		const auto pastEnd = [&](int combo) { return combo > chart.endNote; };
		setTransformedTrigger(skillEvents, combo, false, getTime, pastEnd);
		break;
	}
	case Skill::Trigger::Score:
		setTrigger(scoreTriggers, score);
		break;

	case Skill::Trigger::PerfectCount:
		setTrigger(perfectTriggers, perfect);
		break;

	case Skill::Trigger::StarPerfect:
		setTrigger(starPerfectTriggers, starPerfect);
		break;

	case Skill::Trigger::Chain:
		if (!card.remainingChainTypeNum) {
			skillEvents.emplace(time, SkillOn | card.skillId);
		}
		break;
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
	if (needDelay) {
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
	for (const auto & i : chainTriggers) {
		auto & chainCard = cards[i];
		if (!chainCard.remainingChainTypeNum) {
			continue;
		}
		const auto & chainSkill = chainCard.skill;
		auto type = find(chainSkill.chainTargets.begin(), chainSkill.chainTargets.end(),
			otherCard.type);
		if (type == chainSkill.chainTargets.end()) {
			continue;
		}
		const auto & index = type - chainSkill.chainTargets.begin();
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
