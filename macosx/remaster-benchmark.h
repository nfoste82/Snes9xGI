#ifndef MAC_REMASTER_BENCHMARK_H
#define MAC_REMASTER_BENCHMARK_H

#include "remaster/remaster.h"

// Opt-in live benchmark. Tags are copied into GPU completion callbacks so a
// completed sample always belongs to its producing frame, not the next case.
struct RemasterBenchmarkTag
{
	int caseIndex = -1;
	uint32_t frame = 0;
	bool measured = false;
};
struct RemasterBenchmarkSample
{
	RemasterBenchmarkTag tag;
	RemasterState::PerformanceMetrics metrics;
	double completedTime = 0;
};
bool RemasterBenchmarkActive();
bool RemasterBenchmarkDeterministicPresentation();
RemasterBenchmarkTag GetRemasterBenchmarkTag();
void RecordRemasterBenchmarkSample(const RemasterBenchmarkSample &);
void DrainRemasterPresentation();
void ResetRemasterBenchmarkSampling();
bool SaveRemasterBenchmarkImage(const std::string &path);

#endif
