#pragma once

// Simulation config

#ifndef SIFSIM_DEFAULT_ITERS
#if NDEBUG
#define SIFSIM_DEFAULT_ITERS 100000
#else
#define SIFSIM_DEFAULT_ITERS 100
#endif
#endif

#ifndef SIMULATE_HIT_TIMING
#define SIMULATE_HIT_TIMING 0
#endif

#ifndef FORCE_SKILL_FRAME_DELAY
#define FORCE_SKILL_FRAME_DELAY 0
#endif

#if !FORCE_SKILL_FRAME_DELAY
#ifndef FORCE_SCORE_TRIGGERED_SKILL_FRAME_DELAY
#define FORCE_SCORE_TRIGGERED_SKILL_FRAME_DELAY 1
#endif
#endif


// Optimization config

#ifndef USE_FAST_RANDOM
#define USE_FAST_RANDOM 1
#endif

#if SIMULATE_HIT_TIMING
#ifndef USE_INSERTION_SORT
#define USE_INSERTION_SORT 1
#endif
#endif

#ifndef USE_SSE_4_1_ROUND
#define USE_SSE_4_1_ROUND 1
#endif
