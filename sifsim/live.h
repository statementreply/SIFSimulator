#pragma once

#include <vector>
#include <array>
#include <tuple>
#include <cstdint>
#include <limits>


class Live {
public:
	bool prepare(const char * json);
	int simulate(int id, uint64_t seed = UINT64_C(0xcafef00dd15ea5e5));

public:
	static constexpr double PERFECT_WINDOW = 0.032;
	static constexpr double GREAT_WINDOW = 0.080;
	static constexpr double GOOD_WINDOW = 0.128;
	static constexpr std::array<std::pair<size_t, double>, 7> COMBO_MUL = { {
		{50, 1},
		{100, 1.1},
		{200, 1.15},
		{400, 1.2},
		{600, 1.25},
		{800, 1.3},
		{std::numeric_limits<size_t>::max(), 1.35},
	} };

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

		double time;
		double hitTime;
		int position;
		bool isHold;
		bool isSlide;
		bool isBomb;
		bool gr;
		bool grBegin;

		void effect(int effect) {
			isHold = effect == (int)Effect::Hold || effect == (int)Effect::SlideHold;
			isSlide = effect >= (int)Effect::Slide && effect <= (int)Effect::SlideHold;
			isBomb = effect >= (int)Effect::Bomb1 && effect <= (int)Effect::Bomb9;
		}
	};

private:
	// Settings
	double hiSpeed;
	double hitOffset;
	double sigmaHit;
	double sigmaHoldBegin;
	double sigmaHoldEnd;
	double sigmaSlide;
	double gRateHit;
	double gRateHoldBegin;
	double gRateHoldEnd;
	double gRateSlide;
	double gRateSlideHoldEnd;

	// Unit
	double attr;

	// Chart
	std::vector<Note> combos;
	std::vector<double> notes;
};
