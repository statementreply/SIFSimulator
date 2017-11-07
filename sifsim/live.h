#pragma once

#include <vector>
#include <tuple>


class Live {
public:
	bool prepare(const char * json);
	int simulate();

private:
	struct Note {
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

		double hitTime;
		int position;
		bool isHold;
		bool isSlide;
		bool isBomb;
		bool hitGreat;
		bool releaseGreat;

		void effect(int effect) {
			isHold = effect == (int)Effect::Hold || effect == (int)Effect::Slide;
			isSlide = effect >= (int)Effect::Slide && effect <= (int)Effect::SlideHold;
			isBomb = effect >= (int)Effect::Bomb1 && effect <= (int)Effect::Bomb9;
		}
	};

	struct Event {
		enum class Type {
			Hit,
			SkillOff,
			ActiveSkillOn,
			PassiveSkillOn,
		};

		double time;
		Type type;
		int id;

		bool operator <(const Event & b) const {
			return std::tie(time, type, id) < std::tie(b.time, b.type, b.id);
		}
	};

private:
	std::vector<Note> combos;
	std::vector<double> notes;
};
