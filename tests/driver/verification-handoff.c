/* SPDX-License-Identifier: MIT */
/* 用同步回调模拟“用户帧已完成后下一笔投递失败”，不依赖睡眠或真实 IRQ。 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CONFIG_VIDEO_INTEL_IPU4P 1
#define ENOMEM 12
#define EIO 5
#define ETIMEDOUT 110
#define atomic_read(ptr) (*(ptr))
#define atomic_set(ptr, value) (*(ptr) = (value))
#define READ_ONCE(value) (value)
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define list_for_each_entry(pos, head, member) for ((pos) = (head)->first; (pos); (pos) = (pos)->next)
#define v4l2_subdev_call(sd, group, operation, enable) ((void)(sd), sensor_stream(enable))

struct device { int unused; };
struct media_entity { const char *name; };
struct media_pad { struct media_entity *entity; };
struct v4l2_subdev { int unused; };
struct ipu_isys_csi2 { unsigned int index, nlanes, receiver_errors; };
struct vb2_queue { int errors; };
struct ipu_isys_queue { struct vb2_queue vbq; struct ipu_isys_queue *next; };
struct ipu_isys_pipeline {
    struct ipu_isys_csi2 *csi2;
    struct media_pad *external;
    bool interlaced;
    int verify_active, verify_attempt, verify_round, frames_done, verify_errors;
    int verify_csi_error_bits;
    struct { struct ipu_isys_queue *first; } queues;
};
struct bus_device { struct device dev; };
struct isys { struct bus_device *adev; };
struct ipu_isys_video { struct ipu_isys_pipeline ip; struct isys *isys; };

static int refill_error, early_error, handed_off, completed, queue_errors, log_errors;
static int retries, waits, refills;
static bool complete_during_refill, no_frames;
static struct v4l2_subdev sensor;
static void log_stub(struct device *dev, const char *format, ...) {}
#define dev_info(...) log_stub(__VA_ARGS__)
#define dev_warn(...) log_stub(__VA_ARGS__)
#define dev_err(...) (++log_errors, log_stub(__VA_ARGS__))

static uint64_t ktime_get_mono_fast_ns(void) { return 0; }
static bool trace_front_verification(struct ipu_isys_pipeline *ip) { return true; }
static void trace_front_rx_state(struct ipu_isys_pipeline *ip, const char *stage) {}
static struct v4l2_subdev *media_entity_to_v4l2_subdev(struct media_entity *entity) { return &sensor; }
static void ipu_isys_csi2_error(struct ipu_isys_csi2 *csi2) {}
static int ipu_isys_csi2_rearm_receiver(struct ipu_isys_csi2 *csi2) { return 0; }
static void msleep(int milliseconds) {}
static int sensor_stream(int enable) { retries += enable; return 0; }

static bool wait_verification_round(struct ipu_isys_pipeline *ip)
{
    ++waits;
    ip->frames_done = !no_frames;
    return true;
}

static void start_verification_trace_round(struct ipu_isys_pipeline *ip, int attempt, int round)
{
    ip->verify_attempt = attempt;
    ip->verify_round = round;
}

static void stop_verification_trace(struct ipu_isys_pipeline *ip)
{
    ip->verify_attempt = ip->verify_round = 0;
}

static int stream_capture_refeed(struct ipu_isys_pipeline *ip)
{
    ++refills;
    if (ip->verify_active)
        return early_error;
    ++handed_off;
    /* 第一笔已被接受且完成；随后一笔可能失败。不能把完成的帧重新归还。 */
    completed += complete_during_refill;
    return refill_error;
}

void vb2_queue_error(struct vb2_queue *queue)
{
    ++queue->errors;
    ++queue_errors;
}

#include "lifecycle-under-test.c"

int main(void)
{
    const struct {
        const char *name;
        int handoff_failure, early_failure;
        bool completed, no_frames;
        int result, errors, handoffs;
    } cases[] = {
        { .name = "正常交接", .handoffs = 1 },
        { .name = "首笔用户帧分配失败", .handoff_failure = -ENOMEM, .errors = 2, .handoffs = 1 },
        { .name = "首笔用户帧命令失败", .handoff_failure = -EIO, .errors = 2, .handoffs = 1 },
        { .name = "已有 DONE 帧后投递失败", .handoff_failure = -EIO, .completed = true, .errors = 2, .handoffs = 1 },
        { .name = "隔离期间投递失败", .early_failure = -ENOMEM, .result = -ENOMEM },
        { .name = "无帧耗尽既有重试", .no_frames = true, .result = -ETIMEDOUT },
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct bus_device device = {0};
        struct isys isys = { .adev = &device };
        struct media_entity entity = { .name = "ov5693" };
        struct media_pad external = { .entity = &entity };
        struct ipu_isys_csi2 csi2 = { .index = 2, .nlanes = 2 };
        struct ipu_isys_queue queues[2] = {0};
        queues[0].next = &queues[1];
        struct ipu_isys_video video = {
            .ip = { .csi2 = &csi2, .external = &external, .verify_active = 1,
                .verify_attempt = 1, .verify_round = 1, .queues = { .first = queues } },
            .isys = &isys,
        };
        refill_error = cases[i].handoff_failure;
        early_error = cases[i].early_failure;
        complete_during_refill = cases[i].completed;
        no_frames = cases[i].no_frames;
        handed_off = completed = queue_errors = log_errors = retries = waits = refills = 0;
        int result = verify_stream_start(&video.ip);
        bool ok = result == cases[i].result && queue_errors == cases[i].errors &&
            handed_off == cases[i].handoffs && completed == cases[i].completed &&
            video.ip.verify_active == !cases[i].handoffs &&
            (!queue_errors || (queues[0].vbq.errors == 1 && queues[1].vbq.errors == 1)) &&
            (!cases[i].handoff_failure || log_errors > 0) &&
            (!no_frames || retries == IPU_ISYS_VERIFY_MAX_BOUNCES);
        printf("%s: %s (返回=%d, 队列错误=%d, 已完成=%d, 隔离=%d)\n",
               ok ? "PASS" : "FAIL", cases[i].name, result, queue_errors,
               completed, video.ip.verify_active);
        failures += !ok;
    }
    return failures != 0;
}
