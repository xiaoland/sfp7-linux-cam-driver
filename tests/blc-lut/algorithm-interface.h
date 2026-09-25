// SPDX-License-Identifier: MIT
// No algorithm behavior lives in this interface substitute.
namespace libcamera::ipa::soft {
struct Module {
	using Context = IPAContext;
	using FrameContext = IPAFrameContext;
};
class Algorithm
{
public:
	virtual ~Algorithm() = default;
	virtual int init(IPAContext &, const YamlObject &) { return 0; }
	virtual int configure(IPAContext &, const IPAConfigInfo &) { return 0; }
	virtual void queueRequest(IPAContext &, uint32_t, IPAFrameContext &,
				  const ControlList &) {}
	virtual void prepare(IPAContext &, uint32_t, IPAFrameContext &,
			     DebayerParams *) {}
	virtual void process(IPAContext &, uint32_t, IPAFrameContext &,
			     const SwIspStats *, ControlList &) {}
};
}
