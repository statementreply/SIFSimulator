#pragma once

#include "skill.h"

struct Card {
	int type; // Honoka, Eli, ...
	int category; // μ's, Aqours
	int attribute; // Smile, Pure, Cool
	int status;
	Skill skill;
};
