#pragma once

struct Note {
	double time;
	double holdEndTime;
	double showTime;
	int position;
	int attribute;
	bool isHold;
	bool isSlide;
	bool isBomb;

	// Simulation state
	bool isHoldBeginPerfect;
	double holdBeginHitTime;

	void effect(int effect) {
		isHold = effect == (int)Effect::Hold || effect == (int)Effect::SlideHold;
		isSlide = effect >= (int)Effect::Slide && effect <= (int)Effect::SlideHold;
		isBomb = effect >= (int)Effect::Bomb1 && effect <= (int)Effect::Bomb9;
	}

	enum class Effect {
		Random = 0,
		Normal = 1,
		Event = 2,
		Hold = 3,
		Bomb1 = 4,
		Bomb3 = 5,
		Bomb5 = 6,
		Bomb9 = 7,
		Slide = 11,
		SlideEvent = 12,
		SlideHold = 13,
	};
};
