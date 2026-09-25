/* SPDX-License-Identifier: MIT */
/* 只验证真实 __buf_queue 的资源移交；不证明被模拟的启动函数或 IRQ 行为。 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ENOMEM 12
#define EINVAL 22
#define IPU_ISYS_BUFFER_LIST_FL_INCOMING 1
#define IPU_ISYS_BUFFER_LIST_FL_ACTIVE 2
#define IPU_FW_ISYS_SEND_TYPE_STREAM_CAPTURE 3
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define dev_dbg(...) ((void)0)
#define dev_err(...) ((void)0)
#define WARN_ON(value) warn_stub(value)
#define spin_lock_irqsave(lock, flags) ((flags) = 0)
#define spin_unlock_irqrestore(lock, flags) ((void)(flags))

struct vb2_queue { bool streaming; int errors; };
struct vb2_buffer { struct vb2_queue *vb2_queue; unsigned int num_planes; };
struct ipu_isys_queue { int incoming; };
struct ipu_isys_buffer { int head; bool req; };
struct ipu_isys_pipeline { bool streaming; int nr_streaming, nr_queues; };
struct ipu_isys_video { struct ipu_isys_pipeline ip; int mutex; };
struct ipu_isys_buffer_list { bool owned; };
struct ipu_fw_isys_frame_buff_set_abi { int unused; };
struct isys_fw_msgs { struct ipu_fw_isys_frame_buff_set_abi frame; };

static struct ipu_isys_queue queue;
static struct ipu_isys_buffer buffer;
static struct ipu_isys_video video;
static struct isys_fw_msgs message;
static int outstanding, get_calls, starts, captures, active, start_result;
static bool allocation_failure;

static bool warn_stub(bool value) { return value; }

#define vb2_queue_to_ipu_isys_queue(vbq) (&queue)
#define ipu_isys_queue_to_video(aq) (&video)
#define vb2_buffer_to_ipu_isys_buffer(vb) (&buffer)
#define media_entity_pipeline(...) (&video.ip)
#define to_ipu_isys_pipeline(pipe) (pipe)
#define to_frame_msg_buf(msg) (&(msg)->frame)
#define ipu_fw_isys_dump_frame_buff_set(...) ((void)0)
#define ipu_fw_isys_complex_cmd(...) capture()
#define ipu_put_fw_mgs_buffer(isys, address) put_message(address)

static void mutex_lock(int *mutex) { assert(*mutex == 0); *mutex = 1; }
static void mutex_unlock(int *mutex) { assert(*mutex == 1); *mutex = 0; }
static void list_add(int *head, int *incoming) { ++*incoming; }
static void vb2_queue_error(struct vb2_queue *vbq) { ++vbq->errors; }

static int buffer_list_get(struct ipu_isys_pipeline *ip,
                           struct ipu_isys_buffer_list *list)
{
    assert(queue.incoming == 1);
    --queue.incoming;
    list->owned = true;
    return 0;
}

static struct isys_fw_msgs *ipu_get_fw_msg_buf(struct ipu_isys_pipeline *ip)
{
    ++get_calls;
    if (allocation_failure)
        return NULL;
    ++outstanding;
    return &message;
}

static void put_message(uintptr_t address)
{
    assert(address == (uintptr_t)&message.frame);
    assert(outstanding == 1);
    --outstanding;
}

static void ipu_isys_buffer_list_queue(struct ipu_isys_buffer_list *list,
                                      int destination, int state)
{
    assert(list->owned);
    list->owned = false;
    if (destination == IPU_ISYS_BUFFER_LIST_FL_INCOMING)
        ++queue.incoming;
    else {
        assert(destination == IPU_ISYS_BUFFER_LIST_FL_ACTIVE);
        ++active;
    }
}

static int ipu_isys_stream_start(struct ipu_isys_pipeline *ip,
                                struct ipu_isys_buffer_list *list, bool error)
{
    ++starts;
    ipu_isys_buffer_list_queue(list, start_result ?
        IPU_ISYS_BUFFER_LIST_FL_INCOMING : IPU_ISYS_BUFFER_LIST_FL_ACTIVE, 0);
    return start_result;
}

static void ipu_isys_buffer_list_to_ipu_fw_isys_frame_buff_set(
    struct ipu_fw_isys_frame_buff_set_abi *frame,
    struct ipu_isys_pipeline *ip, struct ipu_isys_buffer_list *list)
{
    assert(list->owned);
}

static int capture(void)
{
    /* 实际驱动必须先把视频缓冲交给 active，再发送固件命令。 */
    assert(active == 1);
    ++captures;
    return 0;
}

#include "queue-under-test.c"

int main(void)
{
    const struct {
        const char *name;
        bool streaming, oom;
        int start_error, expected_starts, expected_gets, expected_active, errors;
    } cases[] = {
        { "延迟启动成功", false, false, 0, 1, 0, 1, 0 },
        { "延迟启动失败", false, false, -1, 1, 0, 0, 1 },
        { "普通捕获", true, false, 0, 0, 1, 1, 0 },
        { "捕获消息分配失败", true, true, 0, 0, 1, 0, 1 },
    };
    int failures = 0;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct vb2_queue vbq = { .streaming = true };
        struct vb2_buffer vb = { .vb2_queue = &vbq };
        queue.incoming = outstanding = get_calls = starts = captures = active = 0;
        video = (struct ipu_isys_video) {
            .ip = { .streaming = cases[i].streaming, .nr_streaming = 1, .nr_queues = 1 },
            .mutex = 1,
        };
        allocation_failure = cases[i].oom;
        start_result = cases[i].start_error;
        __buf_queue(&vb, false);
        bool ok = outstanding == 0 && starts == cases[i].expected_starts &&
            get_calls == cases[i].expected_gets && active == cases[i].expected_active &&
            queue.incoming + active == 1 && vbq.errors == cases[i].errors &&
            captures == (cases[i].streaming && !cases[i].oom) && video.mutex == 1;
        printf("%s: %s (消息=%d, incoming=%d, active=%d, 队列错误=%d)\n",
               ok ? "PASS" : "FAIL", cases[i].name, outstanding,
               queue.incoming, active, vbq.errors);
        failures += !ok;
    }
    return failures != 0;
}
