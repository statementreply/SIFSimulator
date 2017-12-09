#include "configure.h"
#include "nativechar.h"
#include "cmdarg.h"
#include "live.h"
#include "util.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <random>
#include <numeric>
#include "rapidjson/filereadstream.h"

using namespace std;


int Utf8Main(int argc, char * argv[]) try {
	random_device rd;

	if (!parseCmdArg(argc, argv) || g_cmdArg.help) {
		if (g_cmdArg.help) {
			printUsage();
			return 0;
		} else {
			return 1;
		}
	}
	if (!g_cmdArg.iters) {
#if NDEBUG
		g_cmdArg.iters = 100000;
#else
		g_cmdArg.iters = 100;
#endif
	}
	if (!g_cmdArg.seed) {
#if NDEBUG
		g_cmdArg.seed = static_cast<uint64_t>(rd()) ^ static_cast<uint64_t>(rd()) << 32;
#else
		g_cmdArg.seed = UINT64_C(0xcafef00dd15ea5e5);
#endif
	}


	string json;
	rapidjson::FileReadStream is;
	if (g_cmdArg.argumunts.empty() || !*g_cmdArg.argumunts[0] || strcmp(g_cmdArg.argumunts[0], "-") == 0) {
		cin.tie(nullptr);
		json = readAllText(cin);
	} else {
		ifstream fin(ToNative(g_cmdArg.argumunts[0]));
		if (!fin) {
			throw runtime_error("Cannot open file");
		}
		json = readAllText(fin);
	}
	Live live(json);
	//double sum = 0.;
	vector<int> results;
	results.reserve(*g_cmdArg.iters);
	clock_t t0 = clock();
	for (int i = 0; i < *g_cmdArg.iters; i++) {
		results.emplace_back(live.simulate(g_cmdArg.skipIters + i, *g_cmdArg.seed));
		if (!(~i & 0xfff)) {
			clock_t t1 = clock();
			cerr << fixed << setprecision(3) << ((double)(t1 - t0) / CLOCKS_PER_SEC) << endl;
			t0 = t1;
		}
	}
	cerr << "[ Done ]" << endl;
	double avg = accumulate(results.begin(), results.end(), 0.) / results.size();
	double sd = sqrt(accumulate(results.begin(), results.end(), 0., [avg](double s, double x) {
		return s + (x - avg) * (x - avg);
	}) / (results.size() - 1));
	cout << fixed << setprecision(0);
	cout << "Avg\t" << avg << endl;
	cout << "SD\t" << sd << endl;
	auto m = minmax_element(results.begin(), results.end());
	cout << "Min\t" << *m.first << endl;
	cout << "Max\t" << *m.second << endl;
	if (*g_cmdArg.iters >= 10000) {
		auto nth = results.end() - *g_cmdArg.iters / 1000;
		nth_element(results.begin(), nth, results.end());
		cout << "0.1%\t" << *nth << endl;
	}
	return 0;
} catch (exception & e) {
	cerr << "Error: " << e.what() << endl;
	return 1;
} catch (...) {
	cerr << "An unknown error occured" << endl;
	return 1;
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
