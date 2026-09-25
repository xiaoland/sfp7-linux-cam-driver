/* SPDX-License-Identifier: MIT */
/* 执行真实 OPEN/START/STOP/CLOSE 与媒体图启动调用者；硬件和完成通知为受控桩。 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_VIDEO_INTEL_IPU4P 1
#define ENOMEM 12
#define EBUSY 16
#define ENODEV 19
#define EIO 5
#define ETIMEDOUT 110
#define ENOIOCTLCMD 515
#define IPU_ISYS_MAX_STREAMS 4
#define IPU_LIB_CALL_TIMEOUT_JIFFIES 1
#define V4L2_SUBDEV_FORMAT_ACTIVE 1
#define V4L2_SEL_TGT_CROP 1
#define CSI2_BE_PAD_SOURCE 1
#define N_IPU_FW_ISYS_MIPI_DATA_TYPE 0
#define IPU_FW_ISYS_MIPI_STORE_MODE_DISCARD_LONG_HEADER 0
#define IPU_ISL_CSI2_BE 1
#define IPU_ISL_ISA 2
#define IPU_ISYS_SHORT_PACKET_FROM_RECEIVER 1
#define IPU_ISYS_BUFFER_LIST_FL_ACTIVE 1
#define IPU_ISYS_ENTITY_PREFIX "Intel IPU4"
#define atomic_read(ptr) (*(ptr))
#define WARN_ON(value) warn_stub(value)
#define spin_lock_irqsave(lock, flags) ((void)(lock), (flags) = 0)
#define spin_unlock_irqrestore(lock, flags) ((void)(lock), (void)(flags))
#define to_ipu_isys_pipeline(pipe) (pipe)
#define to_frame_msg_buf(msg) (&(msg)->frame)
#define to_stream_cfg_msg_buf(msg) (&(msg)->config)
#define to_dma_addr(msg) ((uintptr_t)(msg))
#define list_for_each_entry(pos, head, member) for ((pos) = (head)->first; (pos); (pos) = (pos)->next)
#define list_for_each_entry_safe(pos, save, head, member) \
    for ((pos) = (head)->first; (pos) && (((save) = (pos)->next), true); (pos) = (save))
#define list_del(head) ((void)(head))
#define list_add(node, head) ((void)(node), (void)(head))
#define v4l2_subdev_call(sd, group, operation, ...) stub_##operation(sd, __VA_ARGS__)

typedef uint32_t u32;
enum ipu_fw_isys_send_type {
    IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN = 1,
    IPU_FW_ISYS_SEND_TYPE_STREAM_START,
    IPU_FW_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE,
    IPU_FW_ISYS_SEND_TYPE_STREAM_STOP,
    IPU_FW_ISYS_SEND_TYPE_STREAM_FLUSH,
    IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE,
};
struct device { int unused; };
struct media_device { int graph_mutex; };
struct media_entity { const char *name; struct { struct media_device *mdev; } graph_obj; };
struct media_pad { struct media_entity *entity; int index; };
struct media_graph { int cursor; };
struct media_entity_enum { bool selected; };
struct v4l2_subdev { const char *name; struct device *dev; struct media_entity entity; };
struct v4l2_subdev_selection { int which, target, pad; struct { int left, top, width, height; } r; };
struct v4l2_subdev_format { struct { int width, height, code; } format; };
struct completion { int type; };
struct ipu_fw_isys_stream_cfg_data_abi {
    int nof_output_pins, compfmt;
    /* Fields used by the real P1 stream config; this remains an ABI subset. */
    int send_irq_sof_discarded, send_resp_sof_discarded;
    int send_irq_eof_discarded, send_resp_eof_discarded;
    struct { struct { int width, height; } input_res; int dt, mapped_dt, mipi_store_mode; } input_pins[1];
    int src, vc, isl_use, nof_input_pins;
    struct { int left_offset, top_offset, right_offset, bottom_offset; } crop[1];
};
struct ipu_fw_isys_frame_buff_set_abi { int marker; };
struct isys_fw_msgs {
    union { struct ipu_fw_isys_stream_cfg_data_abi config; struct ipu_fw_isys_frame_buff_set_abi frame; };
};
struct ipu_isys_buffer_list { int nbufs; };
struct ipu_isys_request { int unused; };
struct ipu_isys_queue;
struct ipu_isys_video;
struct backend;
struct isys;
struct ipu_isys_csi2 { int store_csi2_header; unsigned int stream_count, remote_streams, index; };
struct ipu_isys_pipeline {
    struct isys *isys;
    struct ipu_isys_csi2 *csi2, *tpg;
    struct backend *csi2_be, *csi2_be_soc;
    struct media_pad *external;
    struct { struct ipu_isys_queue *first; } queues;
    struct media_graph graph;
    struct completion stream_open_completion, stream_start_completion, stream_stop_completion, stream_close_completion;
    int source, vc, stream_id, isl_mode, nr_output_pins, stream_handle, error, verify_active;
    bool interlaced;
};
struct ipu_isys_video {
    struct isys *isys;
    struct ipu_isys_pipeline ip;
    struct { struct media_entity entity; } vdev;
    struct media_pad pad;
    bool streaming;
    void (*prepare_firmware_stream_cfg)(struct ipu_isys_video *, struct ipu_fw_isys_stream_cfg_data_abi *);
};
struct ipu_isys_queue {
    struct ipu_isys_queue *next;
    struct ipu_isys_video *av;
    int node;
};
struct backend { struct { struct v4l2_subdev sd; } asd; struct ipu_isys_video av; };
struct bus_device { struct device dev; };
struct isys {
    int lock, stream_opened, short_packet_source;
    struct ipu_isys_pipeline *pipes[IPU_ISYS_MAX_STREAMS];
    struct bus_device *adev;
    struct media_device media_dev;
    struct { struct ipu_isys_video av; } isa;
};

static struct bus_device device;
static struct isys isys;
static struct ipu_isys_video video;
static struct ipu_isys_queue queue;
static struct ipu_isys_csi2 csi2;
static struct v4l2_subdev sensor, receiver;
static struct media_pad external;
static struct isys_fw_msgs message;
static struct ipu_isys_request request;
static int allocations, borrowed, fail_allocation, bad_dump, requests_left, prepare_error;
static int fail_command, fail_ack, ack_error, close_failure, sensor_error, log_errors;
static int graph_refs, enum_refs, sensor_running, power_refs, active_buffers, fw_state;
static int use_stream_stop;
static char commands[32];

static bool warn_stub(bool condition) { return condition; }
static void log_stub(struct device *dev, const char *format, ...) {}
#define dev_dbg(...) log_stub(__VA_ARGS__)
#define dev_info(...) log_stub(__VA_ARGS__)
#define dev_warn(...) log_stub(__VA_ARGS__)
#define dev_err(...) (++log_errors, log_stub(__VA_ARGS__))

static struct ipu_isys_pipeline *media_entity_pipeline(struct media_entity *entity) { return &video.ip; }
static struct media_pad *media_pad_remote_pad_first(struct media_pad *pad) { return &external; }
static int get_external_facing_format(struct ipu_isys_pipeline *ip, struct v4l2_subdev_format *fmt)
{ fmt->format.width = 1920; fmt->format.height = 1080; return 0; }
static int get_comp_format(int code) { return 0; }
static int ipu_isys_mbus_code_to_mipi(int code) { return 0; }
static int v4l2_ctrl_g_ctrl(int control) { return 0; }
static struct ipu_isys_video *ipu_isys_queue_to_video(struct ipu_isys_queue *aq) { return aq->av; }
static void prepare_config(struct ipu_isys_video *av, struct ipu_fw_isys_stream_cfg_data_abi *cfg)
{ cfg->nof_output_pins = 3; }
static void csi_short_packet_prepare_firmware_stream_cfg(struct ipu_isys_pipeline *ip,
                                                        struct ipu_fw_isys_stream_cfg_data_abi *cfg) {}
static void ipu_fw_isys_dump_stream_cfg(struct device *dev, struct ipu_fw_isys_stream_cfg_data_abi *cfg) {}
static void ipu_fw_isys_set_params(struct ipu_fw_isys_stream_cfg_data_abi *cfg) {}
static void ipu_fw_isys_dump_frame_buff_set(struct device *dev,
                                          struct ipu_fw_isys_frame_buff_set_abi *frame, int pins)
{ bad_dump += pins != 3; }
static void ipu_isys_log_csi2_state(struct device *dev, struct ipu_isys_pipeline *ip, const char *stage) {}
static void ipu_isys_csi2_error(struct ipu_isys_csi2 *receiver) {}
static void reinit_completion(struct completion *completion) {}
static int wait_for_completion_timeout(struct completion *completion, int timeout)
{
    video.ip.error = completion->type == ack_error ||
        (completion->type == IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE && close_failure == 3);
    return !(completion->type == fail_ack ||
        (completion->type == IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE && close_failure == 2));
}

static struct isys_fw_msgs *ipu_get_fw_msg_buf(struct ipu_isys_pipeline *ip)
{
    if (++allocations == fail_allocation)
        return NULL;
    assert(!borrowed);
    borrowed = 1;
    memset(&message, 0, sizeof(message)); /* 归还后的消息会被后一次分配复用。 */
    return &message;
}
static void ipu_put_fw_mgs_buffer(struct isys *owner, uintptr_t address)
{
    assert(owner == &isys && address == (uintptr_t)&message && borrowed == 1);
    borrowed = 0;
}
static struct ipu_isys_request *ipu_isys_next_queued_request(struct ipu_isys_pipeline *ip)
{
    if (!requests_left)
        return NULL;
    --requests_left;
    return &request;
}
static int ipu_isys_req_prepare(struct media_device *mdev, struct ipu_isys_request *req,
                               struct ipu_isys_pipeline *ip, struct ipu_fw_isys_frame_buff_set_abi *frame)
{ active_buffers += !prepare_error; return prepare_error; }
static void ipu_isys_buffer_list_to_ipu_fw_isys_frame_buff_set(
    struct ipu_fw_isys_frame_buff_set_abi *frame, struct ipu_isys_pipeline *ip, struct ipu_isys_buffer_list *bl)
{ assert(borrowed && bl->nbufs == 1); frame->marker = 17; }
static void ipu_isys_buffer_list_queue(struct ipu_isys_buffer_list *bl, int flags, int state)
{ assert(flags == IPU_ISYS_BUFFER_LIST_FL_ACTIVE); active_buffers += bl->nbufs; bl->nbufs = 0; }

static int command(enum ipu_fw_isys_send_type type)
{
    const char labels[] = "?OSSTFC";
    size_t length = strlen(commands);
    assert(length + 1 < sizeof(commands));
    commands[length] = labels[type];
    commands[length + 1] = '\0';
    if ((int)type == fail_command || (type == IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE && close_failure == 1))
        return -EIO;
    if (type == IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN)
        fw_state = 1;
    else if (type == IPU_FW_ISYS_SEND_TYPE_STREAM_START || type == IPU_FW_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE)
        fw_state = 2;
    else if (type == IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE && !close_failure)
        fw_state = 0;
    return 0;
}
static int ipu_fw_isys_complex_cmd(struct isys *owner, unsigned int handle, void *payload,
                                  uintptr_t dma, size_t size, enum ipu_fw_isys_send_type type)
{
    assert(borrowed && owner->pipes[handle] == &video.ip);
    if (type == IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN) {
        const struct ipu_fw_isys_stream_cfg_data_abi *config = payload;
        int expected = video.ip.csi2 != NULL;
        assert(config->send_irq_sof_discarded == expected);
        assert(config->send_resp_sof_discarded == expected);
        assert(config->send_irq_eof_discarded == expected);
        assert(config->send_resp_eof_discarded == expected);
    }
    if (type == IPU_FW_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE)
        assert(active_buffers > 0);
    return command(type);
}
static int ipu_fw_isys_simple_cmd(struct isys *owner, unsigned int handle, enum ipu_fw_isys_send_type type)
{ assert(owner->pipes[handle] == &video.ip); return command(type); }

static void mutex_lock(int *lock) { assert(!*lock); *lock = 1; }
static void mutex_unlock(int *lock) { assert(*lock); *lock = 0; }
static int media_graph_walk_init(struct media_graph *graph, struct media_device *mdev) { ++graph_refs; return 0; }
static void media_graph_walk_cleanup(struct media_graph *graph) { --graph_refs; }
static void media_graph_walk_start(struct media_graph *graph, struct media_entity *entity) { graph->cursor = 0; }
static struct media_entity *media_graph_walk_next(struct media_graph *graph)
{ return graph->cursor++ == 0 ? &receiver.entity : NULL; }
static int media_entity_enum_init(struct media_entity_enum *entities, struct media_device *mdev)
{ entities->selected = false; ++enum_refs; return 0; }
static void media_entity_enum_cleanup(struct media_entity_enum *entities) { --enum_refs; }
static void media_entity_enum_set(struct media_entity_enum *entities, struct media_entity *entity) { entities->selected = true; }
static bool media_entity_enum_test(struct media_entity_enum *entities, struct media_entity *entity) { return entities->selected; }
static struct v4l2_subdev *media_entity_to_v4l2_subdev(struct media_entity *entity)
{ return entity == &sensor.entity ? &sensor : &receiver; }
static bool is_media_entity_v4l2_subdev(struct media_entity *entity) { return true; }
static int stub_get_selection(struct v4l2_subdev *sd, void *state, struct v4l2_subdev_selection *selection) { return -EIO; }
static int stub_s_stream(struct v4l2_subdev *sd, int enable)
{
    if (sd == &sensor) {
        if (enable && sensor_error)
            return sensor_error;
        sensor_running = enable;
    } else if (enable)
        ++csi2.stream_count;
    else {
        assert(csi2.stream_count > 0);
        --csi2.stream_count;
    }
    return 0;
}
static bool ipu_isys_csi2_skew_cal_required(struct ipu_isys_csi2 *receiver) { return false; }
static int perform_skew_cal(struct ipu_isys_pipeline *ip) { return 0; }
static int ipu_isys_csi2_rearm_receiver(struct ipu_isys_csi2 *receiver) { return 0; }
static int pm_runtime_resume_and_get(struct device *dev) { ++power_refs; return 0; }
static void pm_runtime_mark_last_busy(struct device *dev) {}
static int pm_runtime_put_autosuspend(struct device *dev) { --power_refs; return 0; }
static void msleep(int milliseconds) {}

#include "lifecycle-under-test.c"

static void reset_case(void)
{
    isys = (struct isys){ .adev = &device };
    sensor = (struct v4l2_subdev){ .name = "ov5693", .dev = &device.dev, .entity = { .name = "ov5693" } };
    receiver = (struct v4l2_subdev){ .name = "Intel IPU4 CSI2", .entity = { .name = "Intel IPU4 CSI2" } };
    external = (struct media_pad){ .entity = &sensor.entity };
    csi2 = (struct ipu_isys_csi2){ .remote_streams = 1, .index = 2 };
    queue = (struct ipu_isys_queue){ .av = &video };
    video = (struct ipu_isys_video){ .isys = &isys, .prepare_firmware_stream_cfg = prepare_config,
        .vdev.entity.graph_obj.mdev = &isys.media_dev,
        .ip = { .isys = &isys, .csi2 = &csi2, .external = &external, .queues.first = &queue,
            .stream_handle = -1, .verify_active = 1,
            .stream_open_completion.type = IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN,
            .stream_start_completion.type = IPU_FW_ISYS_SEND_TYPE_STREAM_START,
            .stream_stop_completion.type = IPU_FW_ISYS_SEND_TYPE_STREAM_STOP,
            .stream_close_completion.type = IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE },
    };
    allocations = borrowed = fail_allocation = bad_dump = requests_left = prepare_error = 0;
    fail_command = fail_ack = ack_error = close_failure = sensor_error = log_errors = 0;
    graph_refs = enum_refs = sensor_running = power_refs = active_buffers = fw_state = 0;
    commands[0] = '\0';
}

int main(void)
{
    const struct {
        const char *name;
        int allocation, command, timeout, ack_error, close_error, prepare;
        bool full_handles, request, pending_request;
        int expected;
        const char *commands;
    } cases[] = {
        { .name = "OPEN 前分配失败", .allocation = 1, .expected = -ENOMEM, .commands = "" },
        { .name = "无可用 handle", .full_handles = true, .expected = -EBUSY, .commands = "" },
        { .name = "OPEN 发送失败", .command = IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN,
          .expected = -EIO, .commands = "O" },
        { .name = "OPEN 超时", .timeout = IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN,
          .expected = -ETIMEDOUT, .commands = "O" },
        { .name = "OPEN 错误响应", .ack_error = IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN,
          .expected = -EIO, .commands = "O" },
        { .name = "OPEN 后分配失败", .allocation = 2, .expected = -ENOMEM, .commands = "OC" },
        { .name = "请求准备失败", .prepare = -EIO, .request = true, .expected = -EIO, .commands = "OC" },
        { .name = "START 发送失败", .command = IPU_FW_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE,
          .expected = -EIO, .commands = "OSC" },
        { .name = "START 超时", .timeout = IPU_FW_ISYS_SEND_TYPE_STREAM_START,
          .expected = -ETIMEDOUT, .commands = "OSC" },
        { .name = "START 错误响应", .ack_error = IPU_FW_ISYS_SEND_TYPE_STREAM_START,
          .expected = -EIO, .commands = "OSC" },
        { .name = "清理 CLOSE 发送失败", .allocation = 2, .close_error = 1,
          .expected = -ENOMEM, .commands = "OC" },
        { .name = "清理 CLOSE 超时", .allocation = 2, .close_error = 2,
          .expected = -ENOMEM, .commands = "OC" },
        { .name = "清理 CLOSE 错误响应", .allocation = 2, .close_error = 3,
          .expected = -ENOMEM, .commands = "OC" },
        { .name = "普通缓冲不消费待处理请求", .pending_request = true, .commands = "OS" },
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        reset_case();
        fail_allocation = cases[i].allocation;
        fail_command = cases[i].command;
        fail_ack = cases[i].timeout;
        ack_error = cases[i].ack_error;
        close_failure = cases[i].close_error;
        prepare_error = cases[i].prepare;
        requests_left = cases[i].request || cases[i].pending_request;
        if (cases[i].full_handles)
            for (size_t j = 0; j < IPU_ISYS_MAX_STREAMS; ++j)
                isys.pipes[j] = &video.ip;
        struct ipu_isys_buffer_list bl = { .nbufs = 1 };
        int result = start_stream_firmware(&video, cases[i].request ? NULL : &bl);
        bool ok = result == cases[i].expected && !borrowed && !bad_dump &&
            strcmp(commands, cases[i].commands) == 0 &&
            isys.stream_opened == (result == 0) &&
            (result == 0 || video.ip.stream_handle == -1) &&
            (!cases[i].pending_request || requests_left == 1) &&
            (!cases[i].close_error || (log_errors > 0 && fw_state != 0));
        printf("%s: %s (返回=%d, 命令=%s, opened=%d, 消息=%d)\n",
               ok ? "PASS" : "FAIL", cases[i].name, result, commands, isys.stream_opened, borrowed);
        failures += !ok;
    }
    /* 用完整调用者验证 sensor 启动失败及正常停止，包含清理失败的有界返回。 */
    const struct {
        bool sensor_failure;
        int close_failure;
        bool stop_failure, stop_timeout;
    } stops[] = {
        {0},
        { .sensor_failure = true },
        { .sensor_failure = true, .close_failure = 1 },
        { .sensor_failure = true, .close_failure = 2 },
        { .sensor_failure = true, .close_failure = 3 },
        { .close_failure = 1 }, { .close_failure = 2 }, { .close_failure = 3 },
        { .sensor_failure = true, .stop_failure = true },
        { .sensor_failure = true, .stop_timeout = true },
    };
    for (size_t i = 0; i < sizeof(stops) / sizeof(stops[0]); ++i) {
        reset_case();
        bool failure = stops[i].sensor_failure;
        sensor_error = failure ? -EIO : 0;
        close_failure = stops[i].close_failure;
        fail_command = stops[i].stop_failure ? IPU_FW_ISYS_SEND_TYPE_STREAM_STOP : 0;
        fail_ack = stops[i].stop_timeout ? IPU_FW_ISYS_SEND_TYPE_STREAM_STOP : 0;
        struct ipu_isys_buffer_list bl = { .nbufs = 1 };
        int result = ipu_isys_video_set_streaming(&video, 1, &bl);
        if (!failure) {
            video.ip.verify_active = 0;
            assert(result == 0 && video.streaming && graph_refs == 1);
            result = ipu_isys_video_set_streaming(&video, 0, NULL);
        }
        bool ok = result == (failure ? -EIO : 0) &&
            strcmp(commands, failure ? "OSTC" : "OSFC") == 0 &&
            !isys.stream_opened && video.ip.stream_handle == -1 &&
            !video.streaming && !borrowed && !graph_refs && !enum_refs &&
            !sensor_running && !csi2.stream_count && !power_refs && !isys.media_dev.graph_mutex &&
            (!close_failure || (log_errors > 0 && fw_state != 0));
        printf("%s: %s / CLOSE 故障=%d / STOP 故障=%d (命令=%s, opened=%d, graph=%d)\n",
               ok ? "PASS" : "FAIL", failure ? "sensor 启动失败" : "正常停流",
               close_failure, stops[i].stop_failure + 2 * stops[i].stop_timeout,
               commands, isys.stream_opened, graph_refs);
        failures += !ok;
    }
    return failures != 0;
}
