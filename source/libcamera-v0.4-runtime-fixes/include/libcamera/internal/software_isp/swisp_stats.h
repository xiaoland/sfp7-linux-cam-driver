/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2023, Linaro Ltd
 *
 * Statistics data format used by the software ISP and software IPA
 */

#pragma once

#include <array>
#include <stdint.h>

namespace libcamera {

/**
 * \brief Struct that holds the statistics for the Software ISP
 *
 * The struct value types are large enough to not overflow.
 * Should they still overflow for some reason, no check is performed and they
 * wrap around.
 */
struct SwIspStats {
	/**
	 * \brief Holds the sum of all sampled red pixels
	 */
	uint64_t sumR_;
	/**
	 * \brief Holds the sum of all sampled green pixels
	 */
	uint64_t sumG_;
	/**
	 * \brief Holds the sum of all sampled blue pixels
	 */
	uint64_t sumB_;
	/**
	 * \brief Brightness-normalized green-channel contrast in the frame centre
	 *
	 * Sum of squared differences between sampled green values two samples
	 * apart, scaled by 1,000,000 and divided by mean green squared. Sampling
	 * uses the central half of the configured window on each axis. Zero can
	 * mean no valid samples, zero mean, or a flat image; it is not a validity
	 * flag. Packed input retains its existing high-eight-bit sampling scale.
	 */
	uint64_t focusContrast;
	/**
	 * \brief Number of bins in the yHistogram
	 */
	static constexpr unsigned int kYHistogramSize = 64;
	/**
	 * \brief Type of the histogram.
	 */
	using Histogram = std::array<uint32_t, kYHistogramSize>;
	/**
	 * \brief A histogram of luminance values
	 */
	Histogram yHistogram;
};

} /* namespace libcamera */
