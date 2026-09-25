/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Deterministic inputs at the real Simple IPA boundary; no copied algorithm state. */
#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

#include <libcamera/controls.h>

#include "ipa/simple/module.h"

namespace libcamera::ipa::soft::test {

inline void require(bool condition, const std::string &message)
{
	if (!condition) {
		std::cerr << message << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

inline void resetContext(IPAContext &context)
{
	context.configuration = {};
	context.activeState = {};
	context.frameContexts.clear();
}

/* Trace values are public requests, with gain in exact hexadecimal notation. */
inline void trace(const std::string &scenario, unsigned int callback,
		  const IPAFrameContext &frame)
{
	std::cout << scenario << ':' << callback << ':';
	if (frame.lens.focusPosition)
		std::cout << *frame.lens.focusPosition;
	else
		std::cout << '-';
	std::cout << ':' << frame.sensor.exposure << ':' << std::hexfloat
		  << frame.sensor.gain << std::defaultfloat << '\n';
}

} /* namespace libcamera::ipa::soft::test */
