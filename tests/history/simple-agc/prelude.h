#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
struct NullLog { template<class T> NullLog &operator<<(const T &) { return *this; } };
#define LOG(a,b) NullLog{}
constexpr unsigned int kExposureBinsCount=5;
constexpr float kExposureOptimal=2.5;
constexpr float kExposureSatisfactory=.2;
struct IPAConfigInfo {};
struct ControlList {};
struct IPAContext { struct { struct { int32_t exposureMin,exposureMax; double againMin,againMax,againMinStep; } agc; } configuration; struct { struct { uint8_t level; } blc; } activeState; };
struct IPAFrameContext { struct { int32_t exposure; double gain; } sensor; };
struct SwIspStats { static constexpr unsigned kYHistogramSize=64; std::array<uint32_t,64> yHistogram{}; };
