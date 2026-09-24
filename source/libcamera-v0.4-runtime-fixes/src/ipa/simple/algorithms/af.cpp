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

constexpr unsigned int kStartupDelayFrames = 8;
constexpr unsigned int kSettleFrames = 1;
constexpr unsigned int kMeasureFrames = 2;
constexpr unsigned int kReferenceFrames = 8;
constexpr unsigned int kRescanCooldownFrames = 300;
constexpr unsigned int kFocusLossFrames = 30;
constexpr unsigned int kRescanRecoveryFrames = 150;
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

	currentPosition_ = info->second.def().get<int32_t>();
	currentPosition_ = std::clamp(currentPosition_, minPosition_, maxPosition_);
	coarseStep_ = std::max<int32_t>(1, (maxPosition_ - minPosition_ + 3) / 4);
	bestPosition_ = currentPosition_;
	bestMetric_ = 0;
	referenceMetric_ = 0;
	filteredMetric_ = 0;
	delayFrames_ = kStartupDelayFrames;
	lossFrames_ = 0;
	recoveryFrames_ = 0;
	focusedFrames_ = 0;
	rescanArmed_ = true;
	stage_ = Stage::Delay;

	LOG(IPASoftAf, Info)
		<< "Autofocus enabled for lens range " << minPosition_ << "-"
		<< maxPosition_ << ", coarse step " << coarseStep_;

	return 0;
}

void Af::moveLens(IPAFrameContext &frameContext, int32_t position)
{
	currentPosition_ = std::clamp(position, minPosition_, maxPosition_);
	frameContext.lens.focusPosition = currentPosition_;
	settleFrames_ = kSettleFrames;
	metricSum_ = 0;
	metricSamples_ = 0;
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
	lossFrames_ = 0;
	recoveryFrames_ = 0;
	focusedFrames_ = 0;
	moveLens(frameContext, minPosition_);

	LOG(IPASoftAf, Debug) << "Starting autofocus scan";
}

void Af::finishCandidate(IPAFrameContext &frameContext, uint64_t metric)
{
	if (metric > bestMetric_) {
		bestMetric_ = metric;
		bestPosition_ = currentPosition_;
	}

	const bool passedPeak = currentPosition_ > bestPosition_ &&
		metric * kScanStopRatioDenominator <
			bestMetric_ * kScanStopRatioNumerator;

	if (currentPosition_ < scanEnd_ && !passedPeak) {
		int32_t next = std::min(scanEnd_, currentPosition_ + scanStep_);
		moveLens(frameContext, next);
		return;
	}

	if (!fineScan_) {
		int32_t coarseBest = bestPosition_;

		fineScan_ = true;
		scanStep_ = std::max<int32_t>(1, coarseStep_ / 4);
		currentPosition_ = std::max(minPosition_, coarseBest - coarseStep_);
		scanEnd_ = std::min(maxPosition_, coarseBest + coarseStep_);
		bestMetric_ = 0;
		bestPosition_ = currentPosition_;

		LOG(IPASoftAf, Debug)
			<< "Coarse autofocus best " << coarseBest
			<< ", refining " << currentPosition_ << "-" << scanEnd_;
		moveLens(frameContext, currentPosition_);
		return;
	}

	frameContext.lens.focusPosition = bestPosition_;
	currentPosition_ = bestPosition_;
	settleFrames_ = kSettleFrames;
	metricSum_ = 0;
	metricSamples_ = 0;
	stage_ = Stage::FinalSettle;

	LOG(IPASoftAf, Info)
		<< "Autofocus selected position " << bestPosition_
		<< " with contrast " << bestMetric_;
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
		if (delayFrames_)
			--delayFrames_;
		if (!delayFrames_)
			startScan(frameContext);
		break;
	case Stage::Settle:
		if (settleFrames_)
			--settleFrames_;
		if (!settleFrames_)
			stage_ = Stage::Measure;
		break;
	case Stage::Measure:
		metricSum_ += metric;
		if (++metricSamples_ == kMeasureFrames)
			finishCandidate(frameContext, metricSum_ / metricSamples_);
		break;
	case Stage::FinalSettle:
		if (settleFrames_)
			--settleFrames_;
		if (!settleFrames_) {
			metricSum_ = 0;
			metricSamples_ = 0;
			stage_ = Stage::Reference;
		}
		break;
	case Stage::Reference:
		metricSum_ += metric;
		if (++metricSamples_ == kReferenceFrames) {
			referenceMetric_ = metricSum_ / metricSamples_;
			filteredMetric_ = referenceMetric_;
			lossFrames_ = 0;
			recoveryFrames_ = 0;
			focusedFrames_ = 0;
			stage_ = Stage::Focused;
		}
		break;
	case Stage::Focused:
		++focusedFrames_;
		filteredMetric_ = filteredMetric_
				  ? (filteredMetric_ * 7 + metric) / 8
				  : metric;

		if (referenceMetric_ &&
		    filteredMetric_ < referenceMetric_ / 2)
			++lossFrames_;
		else
			lossFrames_ = 0;

		if (!rescanArmed_) {
			if (referenceMetric_ &&
			    filteredMetric_ >= referenceMetric_ * 3 / 4 &&
			    filteredMetric_ <= referenceMetric_ * 5 / 4)
				++recoveryFrames_;
			else
				recoveryFrames_ = 0;

			if (focusedFrames_ >= kRescanCooldownFrames &&
			    recoveryFrames_ >= kRescanRecoveryFrames) {
				rescanArmed_ = true;
				lossFrames_ = 0;
				LOG(IPASoftAf, Debug)
					<< "Continuous autofocus rearmed at contrast "
					<< filteredMetric_;
			}
		}

		if (metric > referenceMetric_)
			referenceMetric_ = (referenceMetric_ * 15 + metric) / 16;
		else if (referenceMetric_)
			referenceMetric_ = (referenceMetric_ * 255 + metric) / 256;

		if (rescanArmed_ &&
		    focusedFrames_ >= kRescanCooldownFrames &&
		    lossFrames_ >= kFocusLossFrames) {
			LOG(IPASoftAf, Info)
				<< "Focus contrast dropped from " << referenceMetric_
				<< " to " << filteredMetric_ << ", rescanning";
			startScan(frameContext);
		}
		break;
	}
}

REGISTER_IPA_ALGORITHM(Af, "Af")

} /* namespace ipa::soft::algorithms */

} /* namespace libcamera */
