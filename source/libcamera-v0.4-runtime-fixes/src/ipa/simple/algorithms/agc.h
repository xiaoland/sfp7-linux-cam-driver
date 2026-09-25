/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2024, Red Hat Inc.
 *
 * Exposure and gain
 */

#pragma once

#include "algorithm.h"

namespace libcamera {

namespace ipa::soft::algorithms {

class Agc : public Algorithm
{
public:
	Agc();
	~Agc() = default;

	int configure(IPAContext &context, const IPAConfigInfo &configInfo) override;

	void process(IPAContext &context, const uint32_t frame,
		     IPAFrameContext &frameContext,
		     const SwIspStats *stats,
		     ControlList &metadata) override;

private:
	bool startupAccelerationEnabled_ = true;
	unsigned int validStartupCallbacks_ = 0;

	bool applyStartupBoost(IPAContext &context, IPAFrameContext &frameContext,
			       double exposureMSV);
	void updateExposure(IPAContext &context, IPAFrameContext &frameContext, double exposureMSV);
};

} /* namespace ipa::soft::algorithms */

} /* namespace libcamera */
