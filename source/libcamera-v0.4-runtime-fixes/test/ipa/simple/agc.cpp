/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Real histogram inputs and sensor feedback; no calls to private methods. */

#include <array>
#include <utility>

#include "ipa/simple/algorithms/agc.h"
#include "test_support.h"

using namespace libcamera;
using namespace libcamera::ipa::soft;
using namespace libcamera::ipa::soft::test;

namespace {

SwIspStats histogram(unsigned int bin, unsigned int samples = 100)
{
	SwIspStats stats{};
	stats.yHistogram[bin] = samples;
	return stats;
}

class Fixture
{
public:
	Fixture(const std::string &name, int32_t exposureMax = 10000,
		double gainMax = 8.0, double gainStep = 0.1)
		: name_(name), context_(16)
	{
		resetContext(context_);
		context_.configuration.agc = { 1, exposureMax, 1.0, gainMax, gainStep };
		configure();
	}

	void configure()
	{
		context_.frameContexts.clear();
		require(agc_.configure(context_, {}) == 0, "AGC configure");
	}

	std::pair<int32_t, double> process(const SwIspStats &stats, int32_t exposure = 632,
					 double gain = 1.0)
	{
		auto &frame = context_.frameContexts.alloc(++frameNumber_);
		frame.sensor.exposure = exposure;
		frame.sensor.gain = gain;
		ControlList metadata;
		agc_.process(context_, frameNumber_, frame, &stats, metadata);
		trace(name_, frameNumber_, frame);
		return { frame.sensor.exposure, frame.sensor.gain };
	}

private:
	std::string name_;
	IPAContext context_;
	algorithms::Agc agc_;
	unsigned int frameNumber_ = 0;
};

void budgetAndFeedback()
{
	Fixture fixture("budget");
	for (unsigned int callback = 1; callback <= 100; ++callback) {
		require(fixture.process({}) == std::make_pair(632, 1.0), "empty histogram is inert");
		auto output = fixture.process(histogram(0));
		require(output.first == (callback <= 96 ? 1264 : 695), "96th valid callback still boosts");
		require(output.second == 1.0, "exposure first; repeated feedback is not compounded");
	}
	fixture.configure();
	require(fixture.process(histogram(0)).first == 1264, "configure renews startup budget");
}

void thresholdAndSteadyState()
{
	for (unsigned int lowSamples : { 68, 69, 70, 71, 72 }) {
		Fixture fixture("threshold-" + std::to_string(lowSamples));
		auto stats = histogram(13, lowSamples);
		stats.yHistogram[26] = 100 - lowSamples;
		fixture.process(stats);
		// Persistently bright and then dark input checks one-way startup exit.
		fixture.process(histogram(63));
		require(fixture.process(histogram(0)).first == 695, "startup stays off after bright input");
		for (unsigned int i = 0; i < 400; ++i)
			fixture.process(histogram(i % 64), 1 + i * 19, 1.0 + (i % 7));
	}
}

void limitsAndMinimumSteps()
{
	Fixture nearLimit("near-limit", 1000);
	require(nearLimit.process(histogram(0), 999) == std::make_pair(1000, 1.0), "one startup actuator");
	require(nearLimit.process(histogram(0), 1000) == std::make_pair(1000, 2.0), "gain after exposure limit");
	Fixture saturated("saturated", 1000, 2.0);
	require(saturated.process(histogram(0), 1000, 2.0) == std::make_pair(1000, 2.0), "both limits clamp");
	require(saturated.process(histogram(0), 632).first == 695, "saturation ends acceleration");
	Fixture minimumStep("minimum-step", 1, 8.0, 0.5);
	for (unsigned int i = 0; i < 130; ++i)
		minimumStep.process(histogram(13), 1, 1.0);
	Fixture tiny("tiny-exposure");
	for (unsigned int i = 0; i < 130; ++i)
		tiny.process(histogram(13), 1, 1.0);
}

} /* namespace */

int main()
{
	budgetAndFeedback();
	thresholdAndSteadyState();
	limitsAndMinimumSteps();
	return EXIT_SUCCESS;
}
