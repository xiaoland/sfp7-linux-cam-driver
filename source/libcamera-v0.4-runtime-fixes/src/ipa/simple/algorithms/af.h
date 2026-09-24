/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2026 OpenAI
 *
 * Contrast autofocus for the simple Software ISP
 */

#pragma once

#include <stdint.h>

#include "algorithm.h"

namespace libcamera {

namespace ipa::soft::algorithms {

class Af : public Algorithm
{
public:
	Af() = default;
	~Af() = default;

	int configure(IPAContext &context,
		      const IPAConfigInfo &configInfo) override;
	void process(IPAContext &context, const uint32_t frame,
		     IPAFrameContext &frameContext, const SwIspStats *stats,
		     ControlList &metadata) override;

private:
	enum class Stage {
		Disabled,
		Delay,
		Settle,
		Measure,
		FinalSettle,
		Reference,
		Focused,
	};

	void startScan(IPAFrameContext &frameContext);
	void moveLens(IPAFrameContext &frameContext, int32_t position);
	void finishCandidate(IPAFrameContext &frameContext, uint64_t metric);

	Stage stage_ = Stage::Disabled;
	int32_t minPosition_ = 0;
	int32_t maxPosition_ = 0;
	int32_t currentPosition_ = 0;
	int32_t bestPosition_ = 0;
	int32_t scanEnd_ = 0;
	int32_t scanStep_ = 1;
	int32_t coarseStep_ = 1;
	uint64_t bestMetric_ = 0;
	uint64_t metricSum_ = 0;
	uint64_t referenceMetric_ = 0;
	uint64_t filteredMetric_ = 0;
	unsigned int delayFrames_ = 0;
	unsigned int settleFrames_ = 0;
	unsigned int metricSamples_ = 0;
	unsigned int lossFrames_ = 0;
	unsigned int recoveryFrames_ = 0;
	unsigned int focusedFrames_ = 0;
	bool fineScan_ = false;
	bool rescanArmed_ = true;
};

} /* namespace ipa::soft::algorithms */

} /* namespace libcamera */
