/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2026 OpenAI
 *
 * Contrast autofocus for the simple Software ISP
 */

#include "af.h"

#include <algorithm>

#include <linux/v4l2-controls.h>

#include <libcamera/base/log.h>

namespace libcamera {

LOG_DEFINE_CATEGORY(IPASoftAf)

namespace ipa::soft::algorithms {

namespace {

/* Counts process callbacks, not frame numbers or elapsed sensor time. */
constexpr unsigned int kStartupDelayCallbacks = 8;
constexpr unsigned int kSettleCallbacks = 1;
constexpr unsigned int kCandidateSamples = 2;
constexpr unsigned int kReferenceSamples = 8;
constexpr unsigned int kRescanCooldownCallbacks = 300;
constexpr unsigned int kFocusLossCallbacks = 30;
constexpr unsigned int kRescanRecoveryCallbacks = 150;
constexpr unsigned int kScanStopRatioNumerator = 3;
constexpr unsigned int kScanStopRatioDenominator = 4;

} /* namespace */

int Af::configure([[maybe_unused]] IPAContext &context,
		  const IPAConfigInfo &configInfo)
{
	auto info = configInfo.lensControls.find(V4L2_CID_FOCUS_ABSOLUTE);
	if (info == configInfo.lensControls.end()) {
		stage_ = Stage::Disabled;
		return 0;
	}

	minPosition_ = info->second.min().get<int32_t>();
	maxPosition_ = info->second.max().get<int32_t>();
	if (minPosition_ >= maxPosition_) {
		LOG(IPASoftAf, Warning)
			<< "Invalid lens range " << minPosition_ << "-"
			<< maxPosition_;
		stage_ = Stage::Disabled;
		return 0;
	}

	commandedPosition_ = info->second.def().get<int32_t>();
	commandedPosition_ = std::clamp(commandedPosition_, minPosition_, maxPosition_);
	coarseStep_ = std::max<int32_t>(1, (maxPosition_ - minPosition_ + 3) / 4);
	bestPosition_ = commandedPosition_;
	bestMetric_ = 0;
	referenceMetric_ = 0;
	filteredMetric_ = 0;
	startupCallbacksRemaining_ = kStartupDelayCallbacks;
	lowContrastCallbacks_ = 0;
	recoveryCallbacks_ = 0;
	focusedCallbacks_ = 0;
	rescanArmed_ = true;
	stage_ = Stage::Delay;

	LOG(IPASoftAf, Info)
		<< "Autofocus enabled for lens range " << minPosition_ << "-"
		<< maxPosition_ << ", coarse step " << coarseStep_;

	return 0;
}

/* Queue a raw lens control request; settling does not confirm mechanical arrival. */
void Af::requestLensPosition(IPAFrameContext &frameContext, int32_t position)
{
	commandedPosition_ = std::clamp(position, minPosition_, maxPosition_);
	frameContext.lens.focusPosition = commandedPosition_;
	settleCallbacksRemaining_ = kSettleCallbacks;
	metricSum_ = 0;
	metricSampleCount_ = 0;
	stage_ = Stage::Settle;
}

void Af::startScan(IPAFrameContext &frameContext)
{
	if (stage_ == Stage::Focused)
		rescanArmed_ = false;

	fineScan_ = false;
	scanStep_ = coarseStep_;
	scanEnd_ = maxPosition_;
	bestPosition_ = minPosition_;
	bestMetric_ = 0;
	lowContrastCallbacks_ = 0;
	recoveryCallbacks_ = 0;
	focusedCallbacks_ = 0;
	requestLensPosition(frameContext, minPosition_);

	LOG(IPASoftAf, Debug) << "Starting autofocus scan";
}

void Af::finishCandidate(IPAFrameContext &frameContext, uint64_t metric)
{
	if (metric > bestMetric_) {
		bestMetric_ = metric;
		bestPosition_ = commandedPosition_;
	}

	const bool passedPeak = commandedPosition_ > bestPosition_ &&
		metric * kScanStopRatioDenominator <
			bestMetric_ * kScanStopRatioNumerator;

	if (commandedPosition_ < scanEnd_ && !passedPeak) {
		int32_t next = std::min(scanEnd_, commandedPosition_ + scanStep_);
		requestLensPosition(frameContext, next);
		return;
	}

	if (!fineScan_) {
		int32_t coarseBest = bestPosition_;

		fineScan_ = true;
		scanStep_ = std::max<int32_t>(1, coarseStep_ / 4);
		commandedPosition_ = std::max(minPosition_, coarseBest - coarseStep_);
		scanEnd_ = std::min(maxPosition_, coarseBest + coarseStep_);
		bestMetric_ = 0;
		bestPosition_ = commandedPosition_;

		LOG(IPASoftAf, Debug)
			<< "Coarse autofocus best " << coarseBest
			<< ", refining " << commandedPosition_ << "-" << scanEnd_;
		requestLensPosition(frameContext, commandedPosition_);
		return;
	}

	frameContext.lens.focusPosition = bestPosition_;
	commandedPosition_ = bestPosition_;
	settleCallbacksRemaining_ = kSettleCallbacks;
	metricSum_ = 0;
	metricSampleCount_ = 0;
	stage_ = Stage::FinalSettle;

	LOG(IPASoftAf, Info)
		<< "Autofocus selected position " << bestPosition_
		<< " with contrast " << bestMetric_;
}

/* Update callback age, filtered contrast and consecutive low-contrast count. */
void Af::updateFocusLoss(uint64_t metric)
{
	++focusedCallbacks_;
	filteredMetric_ = filteredMetric_
			  ? (filteredMetric_ * 7 + metric) / 8
			  : metric;

	if (referenceMetric_ &&
	    filteredMetric_ < referenceMetric_ / 2)
		++lowContrastCallbacks_;
	else
		lowContrastCallbacks_ = 0;
}

void Af::updateRescanReadiness()
{
	if (!rescanArmed_) {
		if (referenceMetric_ &&
		    filteredMetric_ >= referenceMetric_ * 3 / 4 &&
		    filteredMetric_ <= referenceMetric_ * 5 / 4)
			++recoveryCallbacks_;
		else
			recoveryCallbacks_ = 0;

		if (focusedCallbacks_ >= kRescanCooldownCallbacks &&
		    recoveryCallbacks_ >= kRescanRecoveryCallbacks) {
			rescanArmed_ = true;
			lowContrastCallbacks_ = 0;
			LOG(IPASoftAf, Debug)
				<< "Continuous autofocus rearmed at contrast "
				<< filteredMetric_;
		}
	}
}

void Af::monitorFocus(IPAFrameContext &frameContext, uint64_t metric)
{
	updateFocusLoss(metric);
	/* Rearming compares against the old reference, before it adapts below. */
	updateRescanReadiness();

	if (metric > referenceMetric_)
		referenceMetric_ = (referenceMetric_ * 15 + metric) / 16;
	else if (referenceMetric_)
		referenceMetric_ = (referenceMetric_ * 255 + metric) / 256;

	if (rescanArmed_ &&
	    focusedCallbacks_ >= kRescanCooldownCallbacks &&
	    lowContrastCallbacks_ >= kFocusLossCallbacks) {
		LOG(IPASoftAf, Info)
			<< "Focus contrast dropped from " << referenceMetric_
			<< " to " << filteredMetric_ << ", rescanning";
		startScan(frameContext);
	}
}

void Af::process([[maybe_unused]] IPAContext &context,
		 [[maybe_unused]] const uint32_t frame,
		 IPAFrameContext &frameContext, const SwIspStats *stats,
		 [[maybe_unused]] ControlList &metadata)
{
	if (stage_ == Stage::Disabled)
		return;

	const uint64_t metric = stats->focusContrast;

	switch (stage_) {
	case Stage::Disabled:
		break;
	case Stage::Delay:
		if (startupCallbacksRemaining_)
			--startupCallbacksRemaining_;
		if (!startupCallbacksRemaining_)
			startScan(frameContext);
		break;
	case Stage::Settle:
		if (settleCallbacksRemaining_)
			--settleCallbacksRemaining_;
		if (!settleCallbacksRemaining_)
			stage_ = Stage::Measure;
		break;
	case Stage::Measure:
		metricSum_ += metric;
		if (++metricSampleCount_ == kCandidateSamples)
			finishCandidate(frameContext, metricSum_ / metricSampleCount_);
		break;
	case Stage::FinalSettle:
		if (settleCallbacksRemaining_)
			--settleCallbacksRemaining_;
		if (!settleCallbacksRemaining_) {
			metricSum_ = 0;
			metricSampleCount_ = 0;
			stage_ = Stage::Reference;
		}
		break;
	case Stage::Reference:
		metricSum_ += metric;
		if (++metricSampleCount_ == kReferenceSamples) {
			referenceMetric_ = metricSum_ / metricSampleCount_;
			filteredMetric_ = referenceMetric_;
			lowContrastCallbacks_ = 0;
			recoveryCallbacks_ = 0;
			focusedCallbacks_ = 0;
			stage_ = Stage::Focused;
		}
		break;
	case Stage::Focused:
		monitorFocus(frameContext, metric);
		break;
	}
}

REGISTER_IPA_ALGORITHM(Af, "Af")

} /* namespace ipa::soft::algorithms */

} /* namespace libcamera */
