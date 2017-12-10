#pragma once

#include "configure.h"
#include <vector>
#include <string>
#include <cstdint>
#include "optional.h"


struct CmdArg {
	bool help = false;
	optional<int> iters;
	uint64_t skipIters = 0;
	optional<int> threads = 0;
	optional<uint64_t> seed;
	std::vector<char *> argumunts;
};

extern CmdArg g_cmdArg;

// Modifies option string
bool parseCmdArg(CmdArg & cmdArg, int argc, char * argv[]);
void printUsage();
