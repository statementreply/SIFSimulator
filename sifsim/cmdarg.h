#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct CmdArg {
	bool help = false;
#if NDEBUG
	int iters = 100000;
#else
	int iters = 100;
#endif
	bool fixedSeed = false;
	uint64_t seed;
	std::vector<char *> argumunts;
};

extern CmdArg g_cmdArg;

// Modifies option string
bool parseCmdArg(int argc, char * argv[]);
void printUsage();
