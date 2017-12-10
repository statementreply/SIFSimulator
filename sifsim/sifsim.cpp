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
#include <thread>
#include <future>
#include <chrono>

using namespace std;
using namespace std::chrono;


int ParseArg(int argc, char * argv[]) {
	if (!parseCmdArg(g_cmdArg, argc, argv) || g_cmdArg.help) {
		if (g_cmdArg.help) {
			printUsage();
			return 0;
		} else {
			return 1;
		}
	}
	if (!g_cmdArg.iters) {
		g_cmdArg.iters = SIFSIM_DEFAULT_ITERS;
	}
	if (!g_cmdArg.seed) {
#if NDEBUG
		random_device rd;
		g_cmdArg.seed = static_cast<uint64_t>(rd()) ^ static_cast<uint64_t>(rd()) << 32;
#else
		g_cmdArg.seed = UINT64_C(0xcafef00dd15ea5e5);
#endif
	}
	return 0;
}


optional<const char *> GetInputFilename() {
	if (g_cmdArg.argumunts.empty()) {
		return nullopt;
	} else if (*g_cmdArg.argumunts[0] == '\0') {
		return nullopt;
	} else if (strcmp(g_cmdArg.argumunts[0], "-") == 0) {
		return nullopt;
	} else {
		return g_cmdArg.argumunts[0];
	}
}


template <class OutputIt>
void RunSimulation(Live & live, uint64_t seed, uint64_t first, uint64_t last, OutputIt result) {
	try {
		for (uint64_t i = first; i != last; ++i) {
			*result++ = live.simulate(i, seed);
		}
	} catch (exception & e) {
		cerr << "Error: " << e.what() << endl;
		quick_exit(1);
	} catch (...) {
		cerr << "An unknown error occured" << endl;
		quick_exit(1);
	}
}


int Utf8Main(int argc, char * argv[]) try {
	int parseRet = ParseArg(argc, argv);
	if (parseRet != 0 || g_cmdArg.help) {
		return parseRet;
	}

	auto inputFilename = GetInputFilename();
	Live live(inputFilename ? CFileWrapper(*inputFilename, "rb") : stdin);
	//double sum = 0.;
	vector<int> results;
	results.resize(*g_cmdArg.iters);

	auto threads = g_cmdArg.threads.value_or(thread::hardware_concurrency());
	if (threads == 0) {
		threads = 1;
	}
	// Last block in current thread
	auto t0 = steady_clock::now();
	uint64_t block = *g_cmdArg.iters / threads;
	vector<Live> lives(threads - 1, live);
	vector<future<void>> futures;
	uint64_t id = g_cmdArg.skipIters;
	auto outit = results.begin();
	for (auto & l : lives) {
		futures.emplace_back(async(launch::async, RunSimulation<decltype(outit)>,
			ref(l), *g_cmdArg.seed, id, id + block, outit));
		id += block;
		outit += block;
	}
	RunSimulation(live, *g_cmdArg.seed, id, g_cmdArg.skipIters + *g_cmdArg.iters, outit);
	for (auto & f : futures) {
		f.get();
	}
	auto t1 = steady_clock::now();
	clog << *g_cmdArg.iters << " simulations completed in "
		<< duration<double>(t1 - t0).count() << " seconds\n";

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
