#pragma once

#include "skill.h"

struct Card {
	int type; // Honoka, Eli, ...
	int category; // μ's, Aqours
	int attribute; // Smile, Pure, Cool
	int baseStatus; // raw + kizuna, for trick (judge) SIS
	int status; // final, for status gain/sync
	Skill skill;
};
