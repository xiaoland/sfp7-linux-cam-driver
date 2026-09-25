// SPDX-License-Identifier: MIT
// Framework-only substitutes. All tested algorithms and their data structures
// are extracted verbatim from the supplied libcamera source tree by run.py.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace libcamera {
struct ControlInfo {
	ControlInfo() = default;
	ControlInfo(float, float, float) {}
};
struct ControlInfoMap {
	using Map = std::map<const int *, ControlInfo>;
};
namespace controls { inline constexpr int Contrast = 1; }
struct ControlList {
	std::optional<double> contrast;
	std::optional<double> get(int) const { return contrast; }
};
struct YamlObject {
	std::optional<int16_t> blackLevel;
	const YamlObject &operator[](const char *) const { return *this; }
	template<typename T> std::optional<T> get() const
	{
		return blackLevel ? std::optional<T>(*blackLevel) : std::nullopt;
	}
};
struct TestLog {
	template<typename T> TestLog &operator<<(const T &) { return *this; }
};
namespace ipa {
struct FrameContext {};
template<typename T> struct FCQueue { explicit FCQueue(unsigned int) {} };
namespace soft { struct IPAConfigInfo {}; }
}
}
#define LOG_DEFINE_CATEGORY(name)
#define LOG(category, level) libcamera::TestLog()
#define REGISTER_IPA_ALGORITHM(type, name)
