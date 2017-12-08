#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <optional>

struct CmdArg {
	bool help = false;
	std::optional<int> iters;
	uint64_t skipIters = 0;
	std::optional<uint64_t> seed;
	std::vector<char *> argumunts;
};

extern CmdArg g_cmdArg;

// Modifies option string
bool parseCmdArg(int argc, char * argv[]);
void printUsage();
