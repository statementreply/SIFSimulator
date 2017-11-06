#include "nativechar.h"


#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include <fstream>
#include <vector>
#include <limits>
#include <array>
#include <cmath>
#include <algorithm>
#include <iostream>
using namespace std;
using namespace rapidjson;
int Test() {
	vector<char> json;
	ifstream fin("Live_s0812.json", ios_base::binary);
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
	struct Combo {
		int time;
		int type;
		int position;
	};
	vector<Combo> combos(doc.Size());
	for (unsigned i = 0; i < doc.Size(); i++) {
		combos[i].position = doc[i]["position"].GetInt();
		combos[i].type = doc[i]["effect"].GetInt();
		double t = doc[i]["timing_sec"].GetDouble();
		if (combos[i].type == 3 || combos[i].type == 13) {
			t += doc[i]["effect_value"].GetDouble();
		}
		combos[i].time = lrint(t * 1e3);
	}
	sort(combos.begin(), combos.end(), [](auto && a, auto && b) {
		return a.time < b.time;
	});
	constexpr array<pair<size_t, double>, 7> ComboMul = { {
		{ 50, 1. },
		{ 100, 1.1 },
		{ 200, 1.15 },
		{ 400, 1.2 },
		{ 600, 1.25 },
		{ 800, 1.3 },
		{ INT_MAX, 1.35 },
	} };
	auto comboIndex = ComboMul.cbegin();
	array<double, 9> weight = { 0. };
	for (size_t i = 0; i < combos.size(); i++) {
		if (i > comboIndex->first) {
			++comboIndex;
		}
		double w = comboIndex->second;
		if (combos[i].type >= 4 && combos[i].type <= 13) {
			w *= 0.5;
		}
		if (combos[i].type == 3 || combos[i].type == 13) {
			w *= 1.25;
		}
		weight[9 - combos[i].position] += w;
	}
	for (size_t i = 0; i < 9; i++) {
		cout << weight[i] << '\t';
	}
	cout << '\n';

	return 0;
}


int Utf8Main(int argc, char * argv[]) {
	return Test();
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
