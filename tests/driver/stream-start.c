/* 执行真实启动和 refeed 函数；仅模拟固件、VB2 与锁，不证明 IRQ/PM 行为。 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ENOMEM 12
#define EINVAL 22
#define ENODATA 61
#define EIO 5
#define IPU_ISYS_BUFFER_LIST_FL_INCOMING 1
#define IPU_ISYS_BUFFER_LIST_FL_ACTIVE 2
#define IPU_ISYS_BUFFER_LIST_FL_SET_STATE 4
#define VB2_BUF_STATE_ERROR 1
#define VB2_BUF_STATE_QUEUED 2
#define IPU_FW_ISYS_SEND_TYPE_STREAM_CAPTURE 1
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define WARN_ON(value) (value)
#define atomic_set(ptr, value) (*(ptr) = (value))
#define dev_dbg(...) ((void)0)
#define dev_info(...) ((void)0)
#define ipu_fw_isys_dump_frame_buff_set(...) ((void)0)
#define to_frame_msg_buf(msg) (&(msg)->frame)
#define to_dma_addr(msg) ((uintptr_t)(msg))
#define ipu_put_fw_mgs_buffer(isys, address) ((void)(isys), put_message(address))
#define ipu_fw_isys_complex_cmd(isys, handle, frame, dma, size, type) capture()

struct media_device { int unused; };
struct isys { int stream_mutex; struct media_device media_dev; };
struct ipu_isys_pipeline { bool streaming, csi2, external, interlaced; int verify_active; };
struct ipu_isys_video { struct ipu_isys_pipeline ip; struct isys *isys; };
struct ipu_isys_buffer_list { int nbufs; };
struct ipu_isys_request { int unused; };
struct ipu_fw_isys_frame_buff_set_abi { int unused; };
struct isys_fw_msgs { struct ipu_fw_isys_frame_buff_set_abi frame; };

static struct isys isys;
static struct ipu_isys_video video;
static struct isys_fw_msgs message;
static struct ipu_isys_request request;
static int incoming, active, returned, borrowed, allocations, captures;
static int starts, stops, verifications, requests, returned_state, requests_left;
static int fail_allocation, capture_error, prepare_error, list_error, start_error, verify_error;

static void mutex_lock(int *lock) { assert(*lock == 0); *lock = 1; }
static void mutex_unlock(int *lock) { assert(*lock == 1); *lock = 0; }
static bool trace_front_verification(struct ipu_isys_pipeline *ip) { return false; }
static void trace_front_rx_state(struct ipu_isys_pipeline *ip, const char *stage) {}
static void start_verification_trace_round(struct ipu_isys_pipeline *ip, int a, int r) {}
static void stop_verification_trace(struct ipu_isys_pipeline *ip) {}

static struct isys_fw_msgs *ipu_get_fw_msg_buf(struct ipu_isys_pipeline *ip)
{
    if (++allocations == fail_allocation)
        return NULL;
    assert(borrowed == 0);
    ++borrowed;
    return &message;
}

static void put_message(uintptr_t address)
{
    assert(address == (uintptr_t)&message.frame && borrowed == 1);
    --borrowed;
}

static int buffer_list_get(struct ipu_isys_pipeline *ip, struct ipu_isys_buffer_list *bl)
{
    bl->nbufs = 0;
    if (list_error)
        return list_error;
    if (!incoming)
        return -ENODATA;
    --incoming;
    bl->nbufs = 1;
    return 0;
}

static void ipu_isys_buffer_list_queue(struct ipu_isys_buffer_list *bl, int flags, int state)
{
    if (flags & IPU_ISYS_BUFFER_LIST_FL_SET_STATE) {
        returned += bl->nbufs;
        returned_state = state;
    } else if (flags & IPU_ISYS_BUFFER_LIST_FL_ACTIVE)
        active += bl->nbufs;
    else
        incoming += bl->nbufs;
    bl->nbufs = 0;
}

static void ipu_isys_buffer_list_to_ipu_fw_isys_frame_buff_set(
    struct ipu_fw_isys_frame_buff_set_abi *set,
    struct ipu_isys_pipeline *ip, struct ipu_isys_buffer_list *bl)
{
    assert(bl->nbufs == 1 && borrowed == 1);
}

static int capture(void)
{
    assert(active > 0 && borrowed == 1);
    ++captures;
    return capture_error;
}

static int ipu_isys_video_set_streaming(struct ipu_isys_video *av, int state,
                                       struct ipu_isys_buffer_list *bl)
{
    assert(isys.stream_mutex == 1);
    if (state) {
        ++starts;
        if (!start_error && bl)
            ipu_isys_buffer_list_queue(bl, IPU_ISYS_BUFFER_LIST_FL_ACTIVE, 0);
        return start_error;
    }
    assert(av->ip.streaming && av->ip.verify_active);
    ++stops;
    return -EIO; /* 清理失败不能覆盖最初的启动错误。 */
}

static struct ipu_isys_request *ipu_isys_next_queued_request(struct ipu_isys_pipeline *ip)
{
    assert(isys.stream_mutex == 1);
    if (!requests_left)
        return NULL;
    --requests_left;
    ++requests;
    return &request;
}

static int ipu_isys_req_prepare(struct media_device *mdev, struct ipu_isys_request *req,
                               struct ipu_isys_pipeline *ip,
                               struct ipu_fw_isys_frame_buff_set_abi *frame)
{
    return prepare_error;
}

static void ipu_isys_req_dispatch(struct media_device *mdev, struct ipu_isys_request *req,
                                 struct ipu_isys_pipeline *ip,
                                 struct ipu_fw_isys_frame_buff_set_abi *frame, uintptr_t dma)
{
    put_message((uintptr_t)frame);
}

static int verify_stream_start(struct ipu_isys_pipeline *ip)
{
    assert(!isys.stream_mutex);
    ++verifications;
    if (!verify_error)
        ip->verify_active = 0;
    return verify_error;
}

static void flush_firmware_streamon_fail(struct ipu_isys_pipeline *ip, int state)
{
    assert(!isys.stream_mutex);
    returned += incoming + active;
    incoming = active = 0;
    returned_state = state;
}

#include "lifecycle-under-test.c"

int main(void)
{
    /* 空 incoming 是正常边界，不能把 -ENODATA 当作启动失败。 */
    video.isys = &isys;
    assert(stream_capture_refeed(&video.ip) == 0 && allocations == 0);
    const struct {
        const char *name;
        int allocation, capture, prepare, list, start, verify, request_count;
        bool error;
        int expected, expected_stops, expected_verifications;
    } cases[] = {
        { .name = "正常启动", .expected_verifications = 1 },
        { .name = "开流前失败", .start = -EIO, .expected = -EIO },
        { .name = "开流后消息分配失败", .allocation = 1, .expected = -ENOMEM, .expected_stops = 1 },
        { .name = "第二笔消息分配失败", .allocation = 2, .expected = -ENOMEM, .expected_stops = 1 },
        { .name = "CAPTURE 投递失败", .capture = -EIO, .expected = -EIO, .expected_stops = 1 },
        { .name = "取缓冲失败", .list = -EINVAL, .expected = -EINVAL, .expected_stops = 1 },
        { .name = "验证失败", .verify = -EIO, .expected = -EIO, .expected_stops = 1, .expected_verifications = 1 },
        { .name = "延迟启动分配失败", .allocation = 1, .error = true, .expected = -ENOMEM, .expected_stops = 1 },
        { .name = "请求消息分配失败", .allocation = 1, .request_count = 1, .expected = -ENOMEM, .expected_stops = 1 },
        { .name = "请求准备失败", .prepare = -EIO, .request_count = 1, .expected = -EIO, .expected_stops = 1 },
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        isys = (struct isys){0};
        video = (struct ipu_isys_video){ .ip = { .csi2 = true, .external = true }, .isys = &isys };
        incoming = 2;
        active = returned = borrowed = allocations = captures = 0;
        starts = stops = verifications = requests = returned_state = 0;
        fail_allocation = cases[i].allocation;
        capture_error = cases[i].capture;
        prepare_error = cases[i].prepare;
        list_error = cases[i].list;
        start_error = cases[i].start;
        verify_error = cases[i].verify;
        requests_left = cases[i].request_count;
        struct ipu_isys_buffer_list bl = { .nbufs = 1 };
        int result = ipu_isys_stream_start(&video.ip, &bl, cases[i].error);
        bool failed = cases[i].expected != 0;
        bool ok = result == cases[i].expected && borrowed == 0 && starts == 1 &&
            stops == cases[i].expected_stops && !isys.stream_mutex &&
            verifications == cases[i].expected_verifications &&
            bl.nbufs == 0 && video.ip.streaming == !failed && video.ip.verify_active == 0 &&
            incoming + active + returned == 3 &&
            (!failed || (returned == 3 && returned_state ==
                (cases[i].error ? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_QUEUED)));
        printf("%s: %s (返回=%d, 停流=%d, 归还=%d, 消息=%d)\n",
               ok ? "PASS" : "FAIL", cases[i].name, result, stops, returned, borrowed);
        failures += !ok;
    }
    return failures != 0;
}
