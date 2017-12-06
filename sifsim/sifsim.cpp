#include "configure.h"
#include "nativechar.h"
#include "live.h"
#include "util.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>


#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "pcg/pcg_random.hpp"
#include <random>
#include <fstream>
#include <vector>
#include <limits>
#include <array>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include "fastrandom.h"
using namespace std;
using namespace rapidjson;
int Test(const char * filename) {
	pcg32 rng(pcg_extras::seed_seq_from<random_device>{});
	vector<char> json;
	ifstream fin(ToNative(filename), ios_base::binary);
	fin.seekg(0, ios_base::end);
	auto len = fin.tellg();
	if (len < 0) {
		return 1;
	}
	fin.seekg(0, ios_base::beg);
	json.resize(static_cast<size_t>(len) + 1);
	fin.read(json.data(), len);
	json.resize(fin.gcount() + 1);
	json.back() = '\0';

	Document doc;
	if (doc.Parse(json.data()).HasParseError()) {
		return 1;
	}
	if (!doc.IsArray()) {
		return 1;
	}
	struct Note {
		int type;
		int position;
		double showTime;
		double comboTime;
	};
	vector<Note> notes(doc.Size());
	normal_distribution<> err(0, 0.015);
	for (unsigned i = 0; i < doc.Size(); i++) {
		auto & note = notes[i];
		note.position = doc[i]["position"].GetInt();
		note.type = doc[i]["effect"].GetInt();
		double t = doc[i]["timing_sec"].GetDouble() - 0.1;
		note.showTime = t - 0.7;
		if (note.type == 3 || note.type == 13) {
			t += doc[i]["effect_value"].GetDouble();
		}
		note.comboTime = t;
		double delta = err(rng);
		if (fabs(delta) > 0.08) {
			delta = copysign(0.08, delta);
		}
		//note.comboTime += delta;
	}
	sort(notes.begin(), notes.end(), [](auto && a, auto && b) {
		return a.comboTime < b.comboTime;
	});
	constexpr array<pair<int, double>, 7> ComboMul = { {
		{ 50, 1 },
		{ 100, 1.1 },
		{ 200, 1.15 },
		{ 400, 1.2 },
		{ 600, 1.25 },
		{ 800, 1.3 },
		{ INT_MAX, 1.35 },
	} };
	auto comboIndex = ComboMul.cbegin();
	array<double, 9> weight = { 0. };
	for (size_t i = 0; i < notes.size(); i++) {
		const auto & note = notes[i];
		if (i >= comboIndex->first) {
			++comboIndex;
		}
		double w = 1;
		w *= 1.2425;
		w *= 1.1;
		w *= comboIndex->second;
		if (note.type >= 11 && note.type <= 13) {
			w *= 0.5;
		}
		if (note.type == 3 || note.type == 13) {
			w *= 1.2425;// 1.25;
		}
		weight[9 - note.position] += w;
	}
	for (size_t i = 0; i < 9; i++) {
		cout << fixed << setprecision(3) << setw(8) << weight[i];
	}
	cout << '\n';
	cout << setprecision(4) << accumulate(weight.begin(), weight.end(), 0.) << '\n';

	return 0;
}


int Utf8Main(int argc, char * argv[]) {
	random_device rd;
	uint64_t seed = static_cast<uint64_t>(rd()) ^ static_cast<uint64_t>(rd()) << 32;

	//return Test(argc >= 2 ? argv[1] : R"(K:\Documents\LL\charts\json\Live_s0812.json)");

	string json;
	if (argc >= 2) {
		json = readAllText(ifstream(ToNative(argv[1]), ios_base::binary));
	} else {
		json = readAllText(cin);
	}
	Live live;
	if (!live.prepare(json.c_str())) {
		return 1;
	}
	//double sum = 0.;
	vector<int> results;
#if NDEBUG
	constexpr int ITERS = 100000;
#else
	constexpr int ITERS = 100;
	seed = 0xcafef00dd15ea5e5ULL;
#endif
	results.reserve(ITERS);
	clock_t t0 = clock();
	for (int i = 0; i < ITERS; i++) {
		results.push_back(live.simulate(i, seed));
		if (!(~i & 0xfff)) {
			clock_t t1 = clock();
			cerr << fixed << setprecision(3) << ((double)(t1 - t0) / CLOCKS_PER_SEC) << endl;
			t0 = t1;
		}
	}
	cerr << "[ Done ]" << endl;
	double avg = accumulate(results.begin(), results.end(), 0.) / results.size();
	double sd = sqrt(accumulate(results.begin(), results.end(), 0., [avg](auto && s, auto && x) {
		return s + (x - avg) * (x - avg);
	}) / (results.size() - 1));
	cout << fixed << setprecision(0);
	cout << "Avg\t" << avg << endl;
	cout << "SD\t" << sd << endl;
	auto m = minmax_element(results.begin(), results.end());
	cout << "Min\t" << *m.first << endl;
	cout << "Max\t" << *m.second << endl;
	if (ITERS >= 10000) {
		auto nth = results.end() - ITERS / 1000;
		nth_element(results.begin(), nth, results.end());
		cout << "0.1%\t" << *nth << endl;
	}
	return 0;
}


#if REQUIRE_CHARSET_CONVERSION

#include <vector>
#include <algorithm>

int NativeMain(int argc, NativeChar * nativeArgv[]) {
	std::vector<char *> argv(argc + 1);
	std::vector<std::vector<char>> args(argc);
	for (int i = 0; i < argc; i++) {
		std::string arg = ToUtf8(nativeArgv[i]);
		args[i].resize(arg.size() + 1);
		std::copy(arg.cbegin(), arg.cend(), args[i].begin());
		args[i].back() = '\0';
		argv[i] = args[i].data();
	}
	argv.back() = nullptr;
	return Utf8Main(argc, argv.data());
}

#else

int main(int argc, char * argv[]) {
	return Utf8Main();
}

#endif
