// SPDX-License-Identifier: MIT
using namespace libcamera;
using namespace libcamera::ipa::soft;
using namespace libcamera::ipa::soft::algorithms;

static void expect(bool value, const char *message)
{
	if (!value)
		throw std::runtime_error(message);
}

struct Fixture {
	IPAContext context{ 4 };
	BlackLevel black;
	Lut lut;
	IPAFrameContext frame{};
	ControlList metadata;
	IPAConfigInfo config;

	Fixture()
	{
		context.configuration = {};
		context.activeState = {};
		black.init(context, YamlObject{});
		configure();
	}

	void configure()
	{
		black.configure(context, config);
		context.activeState.gains = { 1.0, 1.0, 1.0 };
		lut.configure(context, config);
	}

	void histogram(unsigned int bin, int32_t exposure = 100, double gain = 1.0)
	{
		SwIspStats stats{};
		stats.yHistogram.at(bin) = 1000;
		process(stats, exposure, gain);
	}

	void process(const SwIspStats &stats, int32_t exposure, double gain)
	{
		frame.sensor = { exposure, gain };
		black.process(context, 0, frame, &stats, metadata);
	}

	DebayerParams params()
	{
		DebayerParams result{};
		lut.prepare(context, 0, frame, &result);
		return result;
	}

	uint8_t level() const { return context.activeState.blc.level; }
};

static void startup()
{
	Fixture f;
	auto params = f.params();
	std::cout << "startup applied=" << unsigned(f.level())
		  << " LUT[3..6]=" << unsigned(params.red[3]) << ','
		  << unsigned(params.red[4]) << ',' << unsigned(params.red[5])
		  << ',' << unsigned(params.red[6]) << '\n';
	expect(f.level() == 0, "unknown startup applied level must be zero");
	for (unsigned int i = 3; i <= 6; ++i) {
		expect(params.red[i] > 0 && params.green[i] > 0 && params.blue[i] > 0,
		       "rear startup nonzero debayer indices must survive all RGB LUTs");
	}
}

static void empty_then_dark()
{
	Fixture f;
	f.process(SwIspStats{}, 0, 0.0);
	expect(f.level() == 0, "empty histogram must keep unknown applied zero");
	f.histogram(2, 0, 0.0);
	expect(f.level() == 8, "unknown estimate must run even with zero controls");
	auto params = f.params();
	expect(params.red[6] == 0 && params.red[9] > 0,
	       "LUT must follow the first valid nonzero estimate");
}

static void bright_then_dark()
{
	Fixture f;
	f.histogram(20);
	expect(f.level() == 0, "bright no-candidate histogram must keep unknown zero");
	f.histogram(20);
	expect(f.level() == 0, "repeated no-candidate histogram must remain unknown");
	f.histogram(3);
	expect(f.level() == 12, "same controls must not suppress first valid estimate");
}

static void upper_bound_exclusive()
{
	Fixture f;
	f.histogram(4);
	expect(f.level() == 0, "bin at estimator bound must not turn bound into data");
	f.histogram(3);
	expect(f.level() == 12, "estimation bound must remain 16 while applied is zero");
}

static void quantile()
{
	Fixture f;
	SwIspStats stats{};
	stats.yHistogram[0] = 10;
	stats.yHistogram[1] = 9;
	stats.yHistogram[2] = 1;
	stats.yHistogram[30] = 980;
	f.process(stats, 100, 1.0);
	expect(f.level() == 8, "existing two-percent histogram threshold must be retained");
}

static void small_histograms()
{
	for (unsigned int total = 1; total < 50; ++total) {
		Fixture f;
		SwIspStats stats{};
		stats.yHistogram[20] = total;
		f.process(stats, 100, 1.0);
		expect(f.level() == 0, "small bright histogram must keep unknown applied zero");
		stats.yHistogram[20] = 0;
		stats.yHistogram[2] = total;
		f.process(stats, 100, 1.0);
		expect(f.level() == 8, "small histogram threshold must require an actual sample");
		stats.yHistogram[2] = 0;
		stats.yHistogram[0] = total;
		f.process(stats, 101, 1.0);
		expect(f.level() == 0, "small histogram must still permit a valid zero estimate");
	}
}

static void only_decrease()
{
	Fixture f;
	f.histogram(3);
	expect(f.level() == 12, "first estimate");
	f.histogram(1);
	expect(f.level() == 12, "known estimate must skip unchanged controls");
	f.histogram(2, 101);
	expect(f.level() == 8, "changed exposure must permit lower estimate");
	f.histogram(3, 102);
	expect(f.level() == 8, "known estimate must not increase");
	f.histogram(1, 102, 2.0);
	expect(f.level() == 4, "changed gain must permit lower estimate");
	f.histogram(0, 102, 3.0);
	expect(f.level() == 0, "valid zero estimate must be permitted");
	f.histogram(3, 103, 4.0);
	expect(f.level() == 0, "known zero estimate must remain the floor");
}

static void empty_after_valid()
{
	Fixture f;
	f.histogram(2);
	f.process(SwIspStats{}, 101, 2.0);
	expect(f.level() == 8, "empty histogram must preserve valid estimate");
	f.histogram(1, 101, 2.0);
	expect(f.level() == 4, "empty histogram must not consume control history");
}

static void configure_reset()
{
	Fixture f;
	f.histogram(0, 100, 1.0);
	expect(f.level() == 0, "valid floor before configure");
	// soft_simple.cpp resets these before each algorithm configure.
	f.context.configuration = {};
	f.context.activeState = {};
	f.configure();
	expect(f.level() == 0, "reconfigure must start unknown at zero");
	f.histogram(3, 100, 1.0);
	expect(f.level() == 12, "configure must reset estimator bound and control history");
	f.configure();
	f.histogram(2, 100, 1.0);
	expect(f.level() == 8, "standalone configure must also reset private state");
}

static void fixed_helper()
{
	for (uint8_t level : { 0, 8, 16 }) {
		Fixture f;
		f.context.configuration.black.level = level;
		f.configure();
		expect(f.level() == level, "fixed helper level must be applied immediately");
		f.histogram(0);
		f.histogram(3, 101);
		expect(f.level() == level, "histogram must not modify fixed helper level");
		f.configure();
		expect(f.level() == level, "configure must preserve fixed helper level");
		f.context.configuration.black.level.reset();
		f.configure();
		expect(f.level() == 0, "helper removal must return to unknown zero");
		f.histogram(3, 101);
		expect(f.level() == 12, "helper removal must restore automatic estimation");
	}
}

static void fixed_yaml()
{
	for (int16_t level : { 0, 8, 16 }) {
		Fixture f;
		f.context.configuration.black.level = 4;
		YamlObject tuning;
		tuning.blackLevel = level << 8;
		f.black.init(f.context, tuning);
		f.configure();
		expect(f.level() == level, "YAML level must override helper and retain scaling");
		f.histogram(0);
		expect(f.level() == level, "histogram must not modify fixed YAML level");
		f.context.configuration = {};
		f.context.activeState = {};
		f.configure();
		expect(f.level() == level, "YAML level must survive context reset");
	}
}

static void fixed_lut_regression()
{
	Fixture unknown;
	Fixture calibratedZero;
	calibratedZero.context.configuration.black.level = 0;
	calibratedZero.configure();
	auto unknownParams = unknown.params();
	auto zeroParams = calibratedZero.params();
	expect(unknownParams.red == zeroParams.red &&
	       unknownParams.green == zeroParams.green &&
	       unknownParams.blue == zeroParams.blue,
	       "unknown startup must use the complete calibrated-zero LUT");
	Fixture calibrated16;
	calibrated16.context.configuration.black.level = 16;
	calibrated16.configure();
	auto oldParams = calibrated16.params();
	for (unsigned int i = 3; i <= 6; ++i)
		expect(oldParams.red[i] == 0 && oldParams.green[i] == 0 && oldParams.blue[i] == 0,
		       "known 16 must preserve old LUT and reproduce observed black frame");
}

int main(int argc, char **argv)
{
	using Case = std::pair<const char *, void (*)()>;
	const std::vector<Case> cases = {
		{ "startup", startup },
		{ "empty_then_dark", empty_then_dark },
		{ "bright_then_dark", bright_then_dark },
		{ "upper_bound_exclusive", upper_bound_exclusive },
		{ "quantile", quantile },
		{ "small_histograms", small_histograms },
		{ "only_decrease", only_decrease },
		{ "empty_after_valid", empty_after_valid },
		{ "configure_reset", configure_reset },
		{ "fixed_helper", fixed_helper },
		{ "fixed_yaml", fixed_yaml },
		{ "fixed_lut_regression", fixed_lut_regression },
	};
	unsigned int failures = 0;
	for (const auto &test : cases) {
		if (argc > 1 && std::string(argv[1]) == "--startup-only" &&
		    std::string(test.first) != "startup")
			continue;
		try {
			test.second();
			std::cout << "PASS " << test.first << '\n';
		} catch (const std::exception &error) {
			std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
			++failures;
		}
	}
	return failures ? 1 : 0;
}
