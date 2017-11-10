#pragma once

struct Note {
	enum class Effect;

	double time;
	double holdEndTime;
	double showTime;
	int position;
	int attribute;
	bool isHold;
	bool isSlide;
	//bool isBomb;

	void effect(Effect effect) {
		isHold = effect == Effect::Hold || effect == Effect::SlideHold;
		isSlide = effect >= Effect::Slide && effect <= Effect::SlideHold;
		//isBomb = effect >= Effect::Bomb1 && effect <= Effect::Bomb9;
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
