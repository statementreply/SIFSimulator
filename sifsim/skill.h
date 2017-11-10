#pragma once

#include <vector>
#include <utility>

struct Skill {
	enum class Effect;
	enum class Discharge;
	enum class Trigger;
	enum class SubEffect;

	bool valid;
	Effect effect;
	Discharge discharge;
	Trigger trigger;
	int level;
	int maxLevel;
	std::vector<int> effectTargetst;
	std::vector<std::pair<int, int>> triggerTargets;
	std::vector<unsigned char> triggerStatus;

	struct LevelData {
		double effectValue;
		double dischargeTime;
		int triggerValue;
		int activationRate;
	};
	std::vector<LevelData> levels;

	struct SubSkill {
		SubEffect effect;
	};
	std::vector<SubSkill> subSkills;

	enum class Effect {
		None = 0,
		//LevelUp = 1,
		//ScoreUp = 2,
		//SkillUp = 3,
		GreatToPerfect = 4,
		GoodToPerfect = 5,
		//DamageDown = 6,
		//BombClear = 7,
		//ToSkillNote = 8,
		HpRestore = 9,
		//Revive = 10,
		ScorePlus = 11,
		//ScoreRestore = 12,
		//ScorePlusSmile = 1000,
		//ScorePlusPure = 1001,
		//ScorePlusCool = 1002,
		//ScorePlusAll = 1003,
		//ScorePlusMaxHp = 1004,
		//ScorePlusRestHp = 1005,
		SkillRateUp = 2000,
		Mimic = 2100,
		PerfectBonusRatio = 2200,
		PerfectBonusFixedValue = 2201,
		ComboBonusRatio = 2300,
		ComboBonusFixedValue = 2301,
		SyncStatus = 2400,
		GainSkillLevel = 2500,
		GainStatus = 2600,
		//DamageAndScorePlus = 5500,
		//ScoreRestoreWithDamageAndScorePlus = 5501,
		//Damage = 99999,
	};

	enum class Discharge {
		None = 0,
		Immediate = 1,
		Duration = 2,
		//Forever = 3,
		//Limit = 4,
		//DurationLimit = 5,
	};

	enum class Trigger {
		None = 0,
		Time = 1,
		//LifeReduced = 2,
		NotesCount = 3,
		ComboCount = 4,
		Score = 5,
		PerfectCount = 6,
		//GreatCount = 7,
		//GoodCount = 8,
		//BadCount = 9,
		//MissCount = 10,
		//HealCount = 11,
		StarPerfect = 12,
		//StarSuccess = 13,
		//SameTimingPerfect = 14,
		//HealValue = 15,
		Chain = 100,
	};

	enum class SubEffect {
		None = 0,
	};
};
