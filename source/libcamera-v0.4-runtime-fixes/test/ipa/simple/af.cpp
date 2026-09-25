/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Exercise the real AF class through configure/process and lens requests. */

#include <optional>
#include <vector>

#include <linux/v4l2-controls.h>

#include "ipa/simple/algorithms/af.h"
#include "test_support.h"

using namespace libcamera;
using namespace libcamera::ipa::soft;
using namespace libcamera::ipa::soft::test;

namespace {

const ControlId focusId(V4L2_CID_FOCUS_ABSOLUTE, "FocusAbsolute", "v4l2",
			ControlTypeInteger32, ControlId::Direction::In);
const ControlIdMap focusIds{ { V4L2_CID_FOCUS_ABSOLUTE, &focusId } };

IPAConfigInfo lensConfiguration(int32_t minimum, int32_t maximum, int32_t initial)
{
	IPAConfigInfo config;
	config.lensControls = ControlInfoMap({ { &focusId,
		ControlInfo(minimum, maximum, initial) } }, focusIds);
	return config;
}

class Fixture
{
public:
	Fixture(const std::string &name, const IPAConfigInfo &config, unsigned int frameStep = 1)
		: name_(name), context_(16), frameStep_(frameStep)
	{
		configure(config);
	}

	void configure(const IPAConfigInfo &config)
	{
		resetContext(context_);
		require(af_.configure(context_, config) == 0, name_ + ": configure");
	}

	std::optional<int32_t> process(uint64_t contrast)
	{
		frameNumber_ += frameStep_;
		auto &frame = context_.frameContexts.alloc(frameNumber_);
		SwIspStats stats{};
		stats.focusContrast = contrast;
		ControlList metadata;
		af_.process(context_, frameNumber_, frame, &stats, metadata);
		trace(name_, callback_++, frame);
		return frame.lens.focusPosition;
	}

private:
	std::string name_;
	IPAContext context_;
	algorithms::Af af_;
	unsigned int frameStep_;
	unsigned int frameNumber_ = 0;
	unsigned int callback_ = 0;
};

void disabledAndStartup()
{
	for (const auto &config : { IPAConfigInfo{}, lensConfiguration(4, 4, 4),
				    lensConfiguration(5, 4, 4) }) {
		Fixture fixture("disabled", config);
		for (unsigned int i = 0; i < 16; ++i)
			require(!fixture.process(1000), "disabled must not request focus");
		fixture.configure(lensConfiguration(0, 4, 99));
		for (unsigned int i = 0; i < 7; ++i)
			require(!fixture.process(1000), "startup waits seven callbacks");
		require(fixture.process(1000) == 0, "eighth callback requests zero");
		require(!fixture.process(1000), "settle callback does not measure");
		require(!fixture.process(1000), "first measure does not move");
		require(fixture.process(1000) == 1, "second measure advances candidate");
	}
}

void scanShapes()
{
	for (unsigned int shape = 0; shape < 4; ++shape) {
		Fixture fixture("scan-" + std::to_string(shape), lensConfiguration(0, 16, 8));
		int32_t position = 8;
		for (unsigned int i = 0; i < 300; ++i) {
			uint64_t contrast = shape == 0 ? 0 : shape == 1 ? 1000 :
				shape == 2 ? 100 + position * 50 : 1000 - std::abs(position - 8) * 100;
			auto request = fixture.process(contrast);
			if (request) {
				require(*request >= 0 && *request <= 16, "scan stays in lens range");
				position = *request;
			}
		}
		require(position == (shape < 2 ? 0 : shape == 2 ? 16 : 8), "scan selects expected peak/tie");
	}
}

void reconfigureAndFrameGaps()
{
	const auto config = lensConfiguration(0, 4, 2);
	for (unsigned int cut : { 9, 17, 31, 100 }) {
		Fixture interrupted("reconfigure-" + std::to_string(cut), config);
		for (unsigned int i = 0; i < cut; ++i)
			interrupted.process(1000);
		interrupted.configure(config);
		Fixture fresh("fresh-" + std::to_string(cut), config);
		for (unsigned int i = 0; i < 500; ++i)
			require(interrupted.process(1000) == fresh.process(1000), "configure resets scan");
		interrupted.configure({});
		require(!interrupted.process(1000), "disable interrupts focus");
	}
	Fixture dense("dense", config), sparse("sparse", config, 7);
	for (unsigned int i = 0; i < 900; ++i) {
		uint64_t contrast = i < 400 ? 1000 : 10;
		require(dense.process(contrast) == sparse.process(contrast), "count callbacks, not frame gaps");
	}
}

void callbackBoundaries()
{
	for (unsigned int dropAt : { 200, 400 }) {
		Fixture fixture("loss-boundary-" + std::to_string(dropAt), lensConfiguration(0, 4, 2));
		std::optional<unsigned int> firstRescan;
		for (unsigned int i = 0; i < 480; ++i) {
			auto request = fixture.process(i < dropAt ? 1000 : 10);
			if (i >= dropAt && request && !firstRescan)
				firstRescan = i;
		}
		// Flat 0..4 scan establishes its reference on callback 38. Sustained
		// loss before cooldown waits to callback 338; after cooldown, the
		// filtered threshold plus 30 low samples triggers callback 435.
		require(firstRescan == (dropAt == 200 ? 337u : 434u), "cooldown/loss callback boundary");
	}

	for (unsigned int recoveryLength = 165; recoveryLength <= 180; ++recoveryLength) {
		Fixture fixture("recovery-" + std::to_string(recoveryLength), lensConfiguration(0, 4, 2));
		for (unsigned int i = 0; i < 1700; ++i) {
			uint64_t metric = i < 400 ? 1000 : i < 500 ? 10 : i < 1000 ? 0 :
				i < 1000 + recoveryLength ? 1000 : 10;
			fixture.process(metric);
		}
	}
}

void continuousFocus()
{
	Fixture fixture("continuous", lensConfiguration(0, 4, 2));
	for (unsigned int i = 0; i < 3000; ++i) {
		// Hold, brief loss, sustained loss, recovery, then another loss. A fixed
		// tape makes every callback comparable across independent source trees.
		uint64_t metric = 1000;
		if (i == 340 || (i >= 400 && i < 1000) || i >= 2200)
			metric = 10;
		fixture.process(metric);
	}
}

} /* namespace */

int main()
{
	disabledAndStartup();
	scanShapes();
	reconfigureAndFrameGaps();
	callbackBoundaries();
	continuousFocus();
	return EXIT_SUCCESS;
}
