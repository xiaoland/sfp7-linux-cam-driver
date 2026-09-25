// SPDX-License-Identifier: GPL-2.0
//  Copyright (C) 2018 Intel Corporation

#include <linux/delay.h>
#include <linux/timekeeping.h>

#include "ipu.h"
#include "ipu-buttress.h"
#include "ipu-isys.h"
#include "ipu-isys-csi2.h"
#include "ipu-platform-buttress-regs.h"
#include "ipu-platform-isys-csi2-reg.h"
#include "ipu-platform-regs.h"
#include "ipu-trace.h"
#include "ipu-isys-csi2.h"

#define CSI2_UPDATE_TIME_TRY_NUM   3
#define CSI2_UPDATE_TIME_MAX_DIFF  20

static int ipu4p_csi2_ev_correction_params(struct ipu_isys_csi2
					   *csi2, unsigned int lanes)
{
	/*
	 * TBD: add implementation for ipu4p
	 * probably re-use ipu4 implementation
	 */
	return 0;
}


void ipu4p_isys_csi2_get_rx_state(struct ipu_isys_csi2 *csi2,
				  struct ipu4p_isys_csi2_rx_state *state)
{
	state->status = readl(csi2->base + CSI2_REG_CSI_RX_STATUS);
	state->dlanes_hs =
		readl(csi2->base + CSI2_REG_CSI_RX_STATUS_DLANE_HS);
	state->dlanes_lp =
		readl(csi2->base + CSI2_REG_CSI_RX_STATUS_DLANE_LP);
}

static void ipu4p_csi2_log_rx_state(struct ipu_isys_csi2 *csi2, const char *tag)
{
	u32 enable = readl(csi2->base + CSI2_REG_CSI_RX_ENABLE);
	u32 lanes = readl(csi2->base + CSI2_REG_CSI_RX_NOF_ENABLED_LANES);
	u32 config = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);
	u32 status = readl(csi2->base + CSI2_REG_CSI_RX_STATUS);
	u32 hs = readl(csi2->base + CSI2_REG_CSI_RX_STATUS_DLANE_HS);
	u32 lp = readl(csi2->base + CSI2_REG_CSI_RX_STATUS_DLANE_LP);
	/* Raw wide-port offsets; narrow-port programming uses an additional +4. */
	u32 ctermen = readl(csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_CLANE);
	u32 csettle = readl(csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_CLANE);

	dev_dbg(&csi2->isys->adev->dev,
		"csi %u %s: rx enable=0x%x lanes=%u config=0x%x status=0x%x hs=0x%x lp=0x%x raw_dly_0x2c=%u raw_dly_0x30=%u receiver_errors=0x%x\n",
		csi2->index, tag, enable, lanes, config, status, hs, lp,
		ctermen, csettle, csi2->receiver_errors);
}

static void ipu4p_isys_register_errors(struct ipu_isys_csi2 *csi2)
{
	u32 status;
	unsigned int index;
	struct ipu_isys *isys = csi2->isys;
	void __iomem *isys_base = isys->pdata->base;

	index = csi2->index;
	status = readl(isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(index) + 0x8);
	writel(status, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(index) + 0xc);

	status &= 0xffff;
	dev_dbg(&isys->adev->dev, "csi %d rxsync status 0x%x", index, status);
	csi2->receiver_errors |= status;
}

void ipu_isys_csi2_error(struct ipu_isys_csi2 *csi2)
{
	/*
	 * Strings corresponding to CSI-2 receiver errors are here.
	 * Corresponding macros are defined in the header file.
	 */
	static const struct ipu_isys_csi2_error {
		const char *error_string;
		bool is_info_only;
	} errors[] = {
		{"Single packet header error corrected", true},
		{"Multiple packet header errors detected", true},
		{"Payload checksum (CRC) error", true},
		{"FIFO overflow", false},
		{"Reserved short packet data type detected", true},
		{"Reserved long packet data type detected", true},
		{"Incomplete long packet detected", false},
		{"Frame sync error", false},
		{"Line sync error", false},
		{"DPHY recoverable synchronization error", true},
		{"DPHY non-recoverable synchronization error", false},
		{"Escape mode error", true},
		{"Escape mode trigger event", true},
		{"Escape mode ultra-low power state for data lane(s)", true},
		{"Escape mode ultra-low power state exit for clock lane", true},
		{"Inter-frame short packet discarded", true},
		{"Inter-frame long packet discarded", true},
	};
	u32 status;
	unsigned int i;

	/* Register errors once more in case of error interrupts are disabled */
	ipu4p_isys_register_errors(csi2);
	ipu4p_csi2_log_rx_state(csi2, "error snapshot");
	status = csi2->receiver_errors;
	csi2->receiver_errors = 0;

	for (i = 0; i < ARRAY_SIZE(errors); i++) {
		if (status & BIT(i)) {
			if (errors[i].is_info_only)
				dev_dbg(&csi2->isys->adev->dev,
					"csi2-%i info: %s\n",
					csi2->index, errors[i].error_string);
			else
				dev_err_ratelimited(&csi2->isys->adev->dev,
						    "csi2-%i error: %s\n",
						    csi2->index,
						    errors[i].error_string);
		}
	}
}

/*
 * Where each wired receiver port sits in the two CSI GPREG blocks.
 * ipu4p_csi_offsets[] documents the five ports of this platform as s0p3,
 * s1p0, s1p1, s1p2 and s1p3, and their base addresses agree: index 0 at
 * 0x64300 is the fourth 0x100 block of the first serial interface partition,
 * and indices 1 to 4 at 0x6c000 upwards are the first four of the second.
 * Register bits are numbered within a partition's block, so the port number
 * and the CSI-2 index are not the same thing and must not be substituted for
 * one another.
 */
static bool ipu4p_csi2_gpreg_port(unsigned int index, unsigned int *gpreg,
				  unsigned int *port)
{
	static const struct {
		unsigned int gpreg;
		unsigned int port;
	} map[] = {
		{ IPU_GPOFFSET, 3 },
		{ IPU_COMBO_GPOFFSET, 0 },
		{ IPU_COMBO_GPOFFSET, 1 },
		{ IPU_COMBO_GPOFFSET, 2 },
		{ IPU_COMBO_GPOFFSET, 3 },
	};

	if (index >= ARRAY_SIZE(map))
		return false;

	*gpreg = map[index].gpreg;
	*port = map[index].port;

	return true;
}

/*
 * Reset the receiver core of one port and hold the reset asserted, the way
 * mainline's IPU6 input-system driver does at every stream start.  That block
 * has the same layout as this one, soft reset at offset zero and slave reset
 * at four, and it waits a hundred microseconds between asserting and
 * releasing.  This driver never reset the receiver core at all, and runtime
 * fix 0083 pulsed the same bit with the two writes back to back and changed
 * nothing, which is what a reset too short to register looks like.
 *
 * The bit is 0x40 << n for the n-th port of a GPREG block, and the port
 * number comes from the documented map rather than the CSI-2 index: the rear
 * at index 0 is s0p3 and so bit 0x200, and the front at index 2 is s1p1 and
 * so bit 0x80.  The ports of a block share its port configuration, so save
 * and restore it around the reset in case the reset clears it.
 */
static void ipu4p_csi2_soft_reset(struct ipu_isys_csi2 *csi2)
{
	void __iomem *isys_base = csi2->isys->pdata->base;
	u32 asserted, released, cfg;
	unsigned int gpreg, port;
	u32 bit;

	if (!ipu4p_csi2_gpreg_port(csi2->index, &gpreg, &port))
		return;

	bit = 0x40 << port;

	cfg = readl(isys_base + gpreg + CSI2_REG_CSI_GPREG_CR_PORT_CONFIG);

	writel(bit, isys_base + gpreg + CSI2_REG_CSI_GPREG_SOFT_RESET);
	asserted = readl(isys_base + gpreg + CSI2_REG_CSI_GPREG_SOFT_RESET);
	usleep_range(100, 200);
	writel(0, isys_base + gpreg + CSI2_REG_CSI_GPREG_SOFT_RESET);
	released = readl(isys_base + gpreg + CSI2_REG_CSI_GPREG_SOFT_RESET);

	writel(cfg, isys_base + gpreg + CSI2_REG_CSI_GPREG_CR_PORT_CONFIG);

	dev_info(&csi2->isys->adev->dev,
		 "sfp7-csi-srst: csi=%u gpreg=0x%x bit=0x%x asserted=0x%x released=0x%x cfg=0x%x\n",
		 csi2->index, gpreg, bit, asserted, released, cfg);
}

int ipu_isys_csi2_set_stream(struct v4l2_subdev *sd,
			     struct ipu_isys_csi2_timing timing,
			     unsigned int nlanes, int enable)
{
	struct ipu_isys_csi2 *csi2 = to_ipu_isys_csi2(sd);
	struct ipu_isys *isys = csi2->isys;
	void __iomem *isys_base = isys->pdata->base;
	unsigned int dly_shift = 0;
	unsigned int i;
	u32 val, csi2part = 0xffff;

	dev_dbg(&csi2->isys->adev->dev, "csi2 s_stream %d\n", enable);
	ipu4p_csi2_log_rx_state(csi2, "set_stream entry");
	if (!enable) {
		ipu4p_csi2_log_rx_state(csi2, "set_stream disable pre-error");
		ipu_isys_csi2_error(csi2);

		val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);
		val &= ~(CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
			 CSI2_CSI_RX_CONFIG_RELEASE_LP11);
		writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

		writel(0, csi2->base + CSI2_REG_CSI_RX_ENABLE);

		writel(0, isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x4);
		writel(0, isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) +
			   0x10);
		writel
		    (0, isys_base +
		     IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x4);
		writel
		    (0, isys_base +
		     IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x10);
		ipu_isys_csi2_reset_frame_state(csi2);
		ipu4p_csi2_log_rx_state(csi2, "set_stream disable done");
		return 0;
	}

	ipu_isys_csi2_reset_frame_state(csi2);
	ipu4p_csi2_soft_reset(csi2);
	if (csi2->index == 2 && nlanes == 2)
		ipu4p_isys_reapply_front_phy(isys);

	ipu4p_csi2_ev_correction_params(csi2, nlanes);

	/* The front OV5693 receiver uses the narrow-port delay-counter map. */
	if (csi2->index == 2 && nlanes == 2)
		dly_shift = CSI2_REG_CSI_RX_DLY_CNT_NARROW_SHIFT;

	writel(timing.ctermen,
		   csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_CLANE +
		   dly_shift);
	writel(timing.csettle,
		   csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_CLANE +
		   dly_shift);

	for (i = 0; i < nlanes; i++) {
		writel
		    (timing.dtermen,
		     csi2->base + CSI2_REG_CSI_RX_DLY_CNT_TERMEN_DLANE(i) +
		     dly_shift);
		writel
		    (timing.dsettle,
		     csi2->base + CSI2_REG_CSI_RX_DLY_CNT_SETTLE_DLANE(i) +
		     dly_shift);
	}

	if (dly_shift) {
		u32 dly[7];

		for (i = 0; i < ARRAY_SIZE(dly); i++)
			dly[i] = readl(csi2->base +
				       CSI2_REG_CSI_RX_DLY_CNT_TERMEN_CLANE +
				       i * 4);
		dev_info(&csi2->isys->adev->dev,
			 "sfp7-front-timing: shift=%u 2c=%u 30=%u 34=%u 38=%u 3c=%u 40=%u 44=%u\n",
			 dly_shift, dly[0], dly[1], dly[2], dly[3], dly[4],
			 dly[5], dly[6]);
	}

	val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);
	val |= CSI2_CSI_RX_CONFIG_DISABLE_BYTE_CLK_GATING |
	    CSI2_CSI_RX_CONFIG_RELEASE_LP11;
	writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

	writel(nlanes, csi2->base + CSI2_REG_CSI_RX_NOF_ENABLED_LANES);
	writel(CSI2_CSI_RX_ENABLE_ENABLE,
		   csi2->base + CSI2_REG_CSI_RX_ENABLE);

	/*
	 * FW SOF/EOF responses drive frame events and sequence numbers.
	 * Keep receiver errors enabled, but mask CSI sync IRQs to avoid
	 * advancing the same pipeline sequence from two event sources.
	 */

	/* Enable csi2 receiver error interrupts */
	writel(1, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index));
	writel(0, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x14);
	writel(0xffffffff, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0xc);
	writel(1, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x4);
	writel(1, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(csi2->index) + 0x10);

	writel(csi2part, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index));
	writel(0, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x14);
	writel(0xffffffff, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0xc);
	writel(csi2part, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x4);
	writel(csi2part, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(csi2->index) + 0x10);

	ipu4p_csi2_log_rx_state(csi2, "set_stream enable done");
	return 0;
}

static struct ipu_isys_pipeline *
ipu4p_csi2_verifying_pipe(struct ipu_isys_csi2 *csi2)
{
	struct ipu_isys *isys = csi2->isys;
	unsigned int i;

	if (csi2->index != 2)
		return NULL;

	for (i = 0; i < IPU_ISYS_MAX_STREAMS; i++) {
		struct ipu_isys_pipeline *pipe = READ_ONCE(isys->pipes[i]);

		if (pipe && pipe->csi2 == csi2 &&
		    atomic_read(&pipe->verify_active))
			return pipe;
	}

	return NULL;
}

void ipu_isys_csi2_isr(struct ipu_isys_csi2 *csi2)
{
	struct ipu_isys_pipeline *pipe;
	struct ipu4p_isys_csi2_rx_state rx_state;
	unsigned int trace_event;
	u32 ctrl_status;
	u32 new_error_bits;
	u32 status = 0;
	unsigned int bus;
	struct ipu_isys *isys = csi2->isys;
	void __iomem *isys_base = isys->pdata->base;

	bus = csi2->index;
	/* handle ctrl and ctrl0 irq */
	ctrl_status = readl(isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(bus) + 0x8);
	writel(ctrl_status, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(bus) + 0xc);
	dev_dbg(&isys->adev->dev, "csi %d irq_ctrl status 0x%x", bus,
		ctrl_status);

	if (!(ctrl_status & BIT(0)))
		return;

	status = readl(isys_base +
			   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(bus) + 0x8);
	writel(status, isys_base +
		   IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(bus) + 0xc);
	dev_dbg(&isys->adev->dev, "csi %d irq_ctrl0 status 0x%x", bus, status);
	pipe = ipu4p_csi2_verifying_pipe(csi2);
	if (pipe) {
		trace_event = atomic_inc_return(&pipe->verify_csi_events);
		new_error_bits = status & 0xffff;
		if (new_error_bits)
			new_error_bits &= ~atomic_fetch_or(new_error_bits,
							 &pipe->verify_csi_error_bits);
		if (trace_event == 1 || new_error_bits) {
			ipu4p_isys_csi2_get_rx_state(csi2, &rx_state);
			dev_info(&isys->adev->dev,
				 "sfp7-front-verify: mono_ns=%llu attempt=%d round=%d event=csi-irq csi_event=%u irq_ctrl=0x%x irq_ctrl_clear=0x%x irq_ctrl0=0x%x irq_ctrl0_clear=0x%x new_error_bits=0x%x rx_status=0x%x dlanes_hs=0x%x dlanes_lp=0x%x\n",
				 (unsigned long long)ktime_get_mono_fast_ns(),
				 atomic_read(&pipe->verify_attempt),
				 atomic_read(&pipe->verify_round), trace_event,
				 ctrl_status, ctrl_status, status, status,
				 new_error_bits, rx_state.status,
				 rx_state.dlanes_hs, rx_state.dlanes_lp);
		}
	}
	/* register the csi sync error */
	csi2->receiver_errors |= status & 0xffff;
	/* Sync status may be latched even while masked; only FW counts it. */
}

static u64 tunit_time_to_us(struct ipu_isys *isys, u64 time)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(isys->adev->iommu);
	u64 isys_clk = IS_FREQ_SOURCE / adev->ctrl->divisor / 1000000;

	do_div(time, isys_clk);

	return time;
}

static u64 tsc_time_to_tunit_time(struct ipu_isys *isys,
				  u64 tsc_base, u64 tunit_base, u64 tsc_time)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(isys->adev->iommu);
	u64 isys_clk = IS_FREQ_SOURCE / adev->ctrl->divisor / 100000;
	u64 tsc_clk = IPU_BUTTRESS_TSC_CLK / 100000;

	tsc_time *= isys_clk;
	tsc_base *= isys_clk;
	do_div(tsc_time, tsc_clk);
	do_div(tsc_base, tsc_clk);

	return tunit_base + tsc_time - tsc_base;
}

static int update_timer_base(struct ipu_isys *isys)
{
	int rval, i;
	u64 time;

	for (i = 0; i < CSI2_UPDATE_TIME_TRY_NUM; i++) {
		rval = ipu_trace_get_timer(&isys->adev->dev, &time);
		if (rval) {
			dev_err(&isys->adev->dev,
				"Failed to read Tunit timer.\n");
			return rval;
		}
		rval = ipu4_buttress_tsc_read(isys->adev->isp,
					     &isys->tsc_timer_base);
		if (rval) {
			dev_err(&isys->adev->dev,
				"Failed to read TSC timer.\n");
			return rval;
		}
		rval = ipu_trace_get_timer(&isys->adev->dev,
					   &isys->tunit_timer_base);
		if (rval) {
			dev_err(&isys->adev->dev,
				"Failed to read Tunit timer.\n");
			return rval;
		}
		if (tunit_time_to_us(isys, isys->tunit_timer_base - time) <
		    CSI2_UPDATE_TIME_MAX_DIFF)
			return 0;
	}
	dev_dbg(&isys->adev->dev, "Timer base values may not be accurate.\n");
	return 0;
}

/* Extract the timestamp from trace message.
 * The timestamp in the traces message contains two parts.
 * The lower part contains bit0 ~ 15 of the total 64bit timestamp.
 * The higher part contains bit14 ~ 63 of the 64bit timestamp.
 * These two parts are sampled at different time.
 * Two overlaped bits are used to identify if there's roll overs
 * in the lower part during the two samples.
 * If the two overlapped bits do not match, a fix is needed to
 * handle the roll over.
 */
static u64 extract_time_from_short_packet_msg(struct
					      ipu_isys_csi2_monitor_message
					      *msg)
{
	u64 time_h = msg->timestamp_h << 14;
	u64 time_l = msg->timestamp_l;
	u64 time_h_ovl = time_h & 0xc000;
	u64 time_h_h = time_h & (~0xffff);

	/* Fix possible roll overs. */
	if (time_h_ovl >= (time_l & 0xc000))
		return time_h_h | time_l;
	else
		return (time_h_h - 0x10000) | time_l;
}

unsigned int ipu_isys_csi2_get_current_field(struct ipu_isys_pipeline *ip,
					     unsigned int *timestamp)
{
	struct ipu_isys_video *av = container_of(ip, struct ipu_isys_video, ip);
	struct ipu_isys *isys = av->isys;
	unsigned int field = V4L2_FIELD_TOP;

	/*
	 * Find the nearest message that has matched msg type,
	 * port id, virtual channel and packet type.
	 */
	unsigned int i = ip->short_packet_trace_index;
	bool msg_matched = false;
	unsigned int monitor_id;

	update_timer_base(isys);

	if (ip->csi2->index >= IPU_ISYS_MAX_CSI2_LEGACY_PORTS)
		monitor_id = TRACE_REG_CSI2_3PH_TM_MONITOR_ID;
	else
		monitor_id = TRACE_REG_CSI2_TM_MONITOR_ID;

	dma_sync_single_for_cpu(&isys->adev->dev,
				isys->short_packet_trace_buffer_dma_addr,
				IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
				DMA_BIDIRECTIONAL);

	do {
		struct ipu_isys_csi2_monitor_message msg =
		    isys->short_packet_trace_buffer[i];
		u64 sof_time = tsc_time_to_tunit_time(isys,
						      isys->tsc_timer_base,
						      isys->tunit_timer_base,
						      (((u64) timestamp[1]) <<
						       32) | timestamp[0]);
		u64 trace_time = extract_time_from_short_packet_msg(&msg);
		u64 delta_time_us = tunit_time_to_us(isys,
						     (sof_time > trace_time) ?
						     sof_time - trace_time :
						     trace_time - sof_time);

		i = (i + 1) % IPU_ISYS_SHORT_PACKET_TRACE_MSG_NUMBER;

		if (msg.cmd == TRACE_REG_CMD_TYPE_D64MTS &&
		    msg.monitor_id == monitor_id &&
		    msg.fs == 1 &&
		    msg.port == ip->csi2->index &&
#ifdef IPU_VC_SUPPORT
		    msg.vc == ip->vc &&
#endif
		    delta_time_us < IPU_ISYS_SHORT_PACKET_TRACE_MAX_TIMESHIFT) {
			field = (msg.sequence % 2) ?
			    V4L2_FIELD_TOP : V4L2_FIELD_BOTTOM;
			ip->short_packet_trace_index = i;
			msg_matched = true;
			dev_dbg(&isys->adev->dev,
				"Interlaced field ready. field = %d\n", field);
			break;
		}
	} while (i != ip->short_packet_trace_index);
	if (!msg_matched)
		/* We have walked through the whole buffer. */
		dev_dbg(&isys->adev->dev, "No matched trace message found.\n");

	return field;
}

bool ipu_isys_csi2_skew_cal_required(struct ipu_isys_csi2 *csi2)
{
	__s64 link_freq;
	int rval;

	if (!csi2)
		return false;

#ifdef IPU_VC_SUPPORT
	/* Not yet ? */
	if (csi2->remote_streams != csi2->stream_count)
		return false;

#endif
	rval = ipu_isys_csi2_get_link_freq(csi2, &link_freq);
	if (rval)
		return false;

	if (link_freq <= IPU_SKEW_CAL_LIMIT_HZ)
		return false;

	return true;
}

int ipu_isys_csi2_set_skew_cal(struct ipu_isys_csi2 *csi2, int enable)
{
	u32 val;

	val = readl(csi2->base + CSI2_REG_CSI_RX_CONFIG);

	if (enable)
		val |= CSI2_CSI_RX_CONFIG_SKEWCAL_ENABLE;
	else
		val &= ~CSI2_CSI_RX_CONFIG_SKEWCAL_ENABLE;

	writel(val, csi2->base + CSI2_REG_CSI_RX_CONFIG);

	return 0;
}
