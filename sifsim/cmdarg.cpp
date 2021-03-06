#include "cmdarg.h"
#include "util.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <cassert>

using namespace std;

CmdArg g_cmdArg;


void printUsage() {
	cout << R"(Usage: sifsim [OPTION]... [FILE]
Run LLSIF live score simulation.

With no FILE, or when FILE is -, read standard input.

  -n, --iters=NUM         run NUM simulations [default: )" MACRO_STRING(SIFSIM_DEFAULT_ITERS) R"(]
  -s, --seed=NUM          set random seed to NUM
      --skip-iters=NUM    skip NUM iterations before simulation
      --threads=NUM       run in NUM theards [default: 0 (auto)]
  -h, --help              display this help and exit
)";
}


class ErrNoGuard {
public:
	ErrNoGuard() { old = errno; }
	ErrNoGuard(const ErrNoGuard &) = delete;
	ErrNoGuard & operator=(const ErrNoGuard &) = delete;
	~ErrNoGuard() { errno = old; }
private:
	int old;
};


optional<int> strtoi(const char * str, int radix = 10) {
	ErrNoGuard _e;
	char * pend;
	auto i = strtol(str, &pend, radix);
	if (pend == str) return nullopt;
	if (errno == ERANGE) return nullopt;
	if (*pend) return nullopt;
	if (i < numeric_limits<int>::min()) return nullopt;
	if (i > numeric_limits<int>::max()) return nullopt;
	return static_cast<int>(i);
}


optional<uint64_t> strtou64(const char * str, int radix = 10) {
	ErrNoGuard _e;
	char * pend;
	auto u = strtoull(str, &pend, radix);
	static_assert(numeric_limits<decltype(u)>::max() >= numeric_limits<uint64_t>::max(),
		"u should be larger than uint64_t");
	if (pend == str) return nullopt;
	if (errno == ERANGE) return nullopt;
	if (*pend) return nullopt;
	if (u > numeric_limits<uint64_t>::max()) return nullopt;
	return static_cast<uint64_t>(u);
}


bool parseCmdArg(CmdArg & cmdArg, int argc, char * argv[]) {
	const auto locateArg = [argv](bool isLongOpt, char * parg, int &i) {
		if (isLongOpt) {
			char * pos = strchr(parg, '=');
			if (pos) {
				*pos++ = '\0';
			} else {
				pos = argv[++i];
			}
			return pos;
		} else {
			return parg[1] ? parg + 1 : argv[++i];
		}
	};

	bool acceptOpt = true;
	for (int i = 1; i < argc; i++) {
		char * parg = argv[i];
		if (!(acceptOpt && parg[0] == '-' && parg[1] != '\0')) {
			cmdArg.argumunts.emplace_back(parg);
			continue;
		}

		++parg;
		const char * pval;
		bool isLongOpt = false;
		bool haveArg = false;

	_shortOpt:
		switch (*parg) {
		case '\0':
			continue;
		case '-':
			isLongOpt = true;
			++parg;
			goto _longOpt;
		case 'h':
		case '?':
			goto _help;
		case 'n':
			goto _iters;
		case 's':
			goto _seed;
		default:
			goto _badOpt;
		}
		assert(false);
		throw logic_error("Bug detected. Please contact software developer.\n  (" FILE_LOC ")");

	_longOpt:
		if (*parg == '\0') {
			acceptOpt = false;
			continue;

		} else if (strcmp(parg, "help") == 0) {
		_help:
			cmdArg.help = true;

		} else if (strcmp(parg, "iters") == 0) {
		_iters:
			haveArg = true;
			pval = locateArg(isLongOpt, parg, i);
			if (!pval) goto _noArg;
			auto n = strtoi(pval);
			if (!n || *n <= 0) goto _badArg;
			cmdArg.iters = *n;

		} else if (strcmp(parg, "seed") == 0) {
		_seed:
			haveArg = true;
			pval = locateArg(isLongOpt, parg, i);
			if (!pval) goto _noArg;
			auto u = strtou64(pval, 0);
			if (!u) goto _badArg;
			cmdArg.seed = *u;

		} else if (strcmp(parg, "skip-iters") == 0) {
			haveArg = true;
			pval = locateArg(isLongOpt, parg, i);
			if (!pval) goto _noArg;
			auto u = strtou64(pval);
			if (!u) goto _badArg;
			cmdArg.skipIters = *u;

		} else if (strcmp(parg, "threads") == 0) {
			haveArg = true;
			pval = locateArg(isLongOpt, parg, i);
			if (!pval) goto _noArg;
			auto n = strtoi(pval);
			if (!n || *n < 0) goto _badArg;
			if (*n == 0) {
				cmdArg.threads = nullopt;
			} else {
				cmdArg.threads = *n;
			}

		} else {
			goto _badOpt;
		}

		if (isLongOpt || haveArg) {
			continue;
		} else {
			++parg;
			goto _shortOpt;
		}
		assert(false);
		throw logic_error("Bug detected. Please contact software developer.\n  (" FILE_LOC ")");

	_badOpt:
		if (isLongOpt) {
			cerr << "sifsim: unknown option --" << parg << "\n";
		} else {
			cerr << "sifsim: unknown option --" << parg << "\n";
		}
		cerr << "Try 'sifsim --help' for more information.\n";
		return false;
	_noArg:
		if (isLongOpt) {
			cerr << "sifsim: option --" << parg << " requires an argument\n";
		} else {
			cerr << "sifsim: option -" << *parg << " requires an argument\n";
		}
		cerr << "Try 'sifsim --help' for more information.\n";
		return false;
	_badArg:
		if (isLongOpt) {
			cerr << "sifsim: invalid argument for option --" << parg << ": '" << pval << "'\n";
		} else {
			cerr << "sifsim: invalid argument for option -" << *parg << ": '" << pval << "'\n";
		}
		return false;
	}

	return true;
}
