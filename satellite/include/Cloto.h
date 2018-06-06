#ifndef CLOTO_INCLUDE
#define CLOTO_INCLUDE

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
#endif
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define CLOTO_DECL_ALIGN(bits) __declspec(align(bits))
#else
// TODO
#error "Define __declspec(align()) for this compiler!"
#endif

//--------------------------------------------------------------------------------------------------
// Atomics

void        cloto_memory_barrier();
void        cloto_compiler_barrier();
// Returns the atomic value pre-add
uint32_t    cloto_atomic_fetch_add_u32 ( uint32_t* atomic, uint32_t value );
uint32_t    cloto_atomic_fetch_add_i32 ( uint32_t* atomic, uint32_t value );
// Returns whether the operation was successful. The actual read is written into the expected_read param.
bool        cloto_atomic_compare_exchange_u32 ( uint32_t* atomic, uint32_t* expected_read, uint32_t conditional_write );

//--------------------------------------------------------------------------------------------------
// Job

typedef void ClotoJobRoutine ( void* args );
#define CLOTO_JOB(name) void name (void* args)

typedef struct {
    ClotoJobRoutine* routine;
    void* args;
} ClotoJob;

void cloto_job_create ( ClotoJob* job, ClotoJobRoutine* routine, void* args );

//--------------------------------------------------------------------------------------------------
// Work Queue (Deque)

typedef struct ClotoWorkQueue {
    char _p0[64];
    ClotoJob* jobs;
    char _p1[64];
    uint32_t top;
    char _p2[64];
    uint32_t bottom;
    char _p3[64];
    uint32_t mask;
    char _p4[64];
} ClotoWorkQueue;

bool        cloto_workqueue_create ( ClotoWorkQueue* queue, uint32_t capacity );
void        cloto_workqueue_destroy ( ClotoWorkQueue* queue );
bool        cloto_workqueue_push ( ClotoWorkQueue* queue, const ClotoJob* job );
bool        cloto_workqueue_pop ( ClotoWorkQueue* queue, ClotoJob* job );
bool        cloto_workqueue_steal ( ClotoWorkQueue* queue, ClotoJob* job );

//--------------------------------------------------------------------------------------------------
// Task Queue (SPMC)
// TODO: implement this and use it in slave groups instead of using a shared work queue

//typedef struct {
//    char _p0[64];
//    ClotoJob* jobs;
//    char _p1[64];
//    uint32_t top;
//    char _p2[64];
//    uint32_t bottom;
//    char _p3[64];
//    uint32_t mask;
//    char _p4[64];
//} ClotoTaskQueue;

//bool cloto_taskqueue_create ( ClotoTaskQueue* queue, uint32_t capacity );
//void cloto_taskqueue_destroy ( ClotoTaskQueue* queue );
//bool cloto_taskqueue_push ( ClotoTaskQueue* queue, const ClotoJob* job );
//bool cloto_taskqueue_pop ( ClotoTaskQueue* queue, ClotoJob* job );

//--------------------------------------------------------------------------------------------------
// Messaging system:
// messages get pushed in a single thread-owned queue by other threads
// owner thread processes messages between jobs
// command messages are immedaitely executed and handled
// user messages are deferred to user handling, meaning, they get copied in another thread local queue
// the user can then query this queue for new messages and handle them on his own.
// messages in the public per-thread shared queue can be queried for completion.
// command messages are automatically maked as completed immediately after being executed
// user messages are marked as completed whenever moved from the shared queue to the thread-local user-accessible queue.
// further bookkeeping of user messages is left to the user.
// querying for completion is done by simply increasing a counter each time a message is processed and checking the message id against that when needed.

typedef enum {
    CLOTO_MSG_JOB_NO_ARGS = 0,
    CLOTO_MSG_JOB_REMOTE_ARGS,
    CLOTO_MSG_JOB_LOCAL_ARGS,
    CLOTO_MSG_USER,
} ClotoMessageType;

typedef struct {
    char data[56];
    // 2^6 == 64
    uint32_t size : 6;
    // ClotoMessageType
    uint32_t type : 2;
    // unused
    uint32_t _pad0 : 24;
    // used internally by the MessageQueue
    uint32_t queue_local_id;
} ClotoMessage;

typedef struct {
    char data[56];
    uint64_t data_size;
} ClotoUserMessage;

//--------------------------------------------------------------------------------------------------
// Message Queue (MPSC)

typedef struct {
    char _p0[64];
    ClotoMessage* messages;
    char _p1[64];
    uint32_t top;
    char _p2[64];
    uint32_t bottom;
    char _p3[64];
    uint32_t mask;
    char _p4[64];
    uint32_t processed;
    char _p5[64];
} ClotoMessageQueue;

bool        cloto_msgqueue_create ( ClotoMessageQueue* queue, uint32_t capacity );
void        cloto_msgqueue_destroy ( ClotoMessageQueue* queue );
bool        cloto_msgqueue_push ( ClotoMessageQueue* queue, const void* data, uint32_t data_size, ClotoMessageType type, uint32_t* id );
bool        cloto_msgqueue_pop ( ClotoMessageQueue* queue, ClotoMessage* msg );

//--------------------------------------------------------------------------------------------------
// User-level Message Queue (SPSC)

typedef struct {
    ClotoUserMessage* messages;
    uint32_t top;
    uint32_t bottom;
    uint32_t mask;
} ClotoUserMessageQueue;

bool        cloto_usermsgqueue_create ( ClotoUserMessageQueue* queue, uint32_t capacity );
void        cloto_usermsgqueue_destroy ( ClotoUserMessageQueue* queue );
bool        cloto_usermsgqueue_push ( ClotoUserMessageQueue* queue, const ClotoMessage* msg );
bool        cloto_usermsgqueue_pop ( ClotoUserMessageQueue* queue, ClotoUserMessage* msg );

//--------------------------------------------------------------------------------------------------
// Thread
// Simple standard thread interface. Each thread has a MessageQueue (and its user dedicated conterpart).
// Call cloto_thread_register() only from threads that were not stared using the Cloto API (e.g. the main thread).
// If you do that, that thread should also call cloto_thread_dispose() before it exits (doesn't really do much in case of the main thread).

typedef void ClotoThreadRoutine ( void* args );

typedef enum {
    CTS_IDLE,
    CTS_RUNNING,
    CTS_INVALID
} ClotoThreadState;

typedef struct {
#ifdef _WIN32
    HANDLE handle;
#else
    pthread_t handle;
#endif
    ClotoThreadRoutine* routine;
    void* args;
    ClotoMessageQueue msg_queue;
    ClotoUserMessageQueue user_msg_queue;
} ClotoThread;

void            cloto_thread_register();
void            cloto_thread_dispose();

ClotoThread*    cloto_thread_get();
bool            cloto_thread_create ( ClotoThread* thread, ClotoThreadRoutine* routine, void* args, uint32_t msg_queue_cap, uint32_t user_msg_queue_cap );
bool            cloto_thread_join ( ClotoThread* thread );
bool            cloto_thread_destroy ( ClotoThread* thread );
void            cloto_thread_yield();
uint32_t        cloto_thread_send_message ( ClotoThread* destinatary, const void* data, size_t size, ClotoMessageType type );
bool            cloto_thread_query_message_status ( ClotoThread* destinatary, uint32_t msg_id );

//--------------------------------------------------------------------------------------------------
// Slave (job fetch-execute only worker thread)
// Since they only fetch and execute jobs, a single queue is used. The real world use cases can be many (infinite)
// and each has its own peculiarity and optimal scheduling. This is an approximation.
// The typical use case for this is static, push-all-jobs-then-execute workloads.
// If this is the case, after executon it is also possible to reset the queue and redo all the jobs.

typedef struct ClotoSlaveGroup ClotoSlaveGroup;
typedef struct {
    ClotoThread thread;
    ClotoSlaveGroup* group;
    uint32_t id;
    uint32_t stop_flag;
} ClotoSlave;

bool        cloto_slave_create ( ClotoSlave* slave, ClotoSlaveGroup* group, uint32_t id );
bool        cloto_slave_join ( ClotoSlave* slave );
bool        cloto_slave_destroy ( ClotoSlave* slave );

typedef struct ClotoSlaveGroup {
    ClotoWorkQueue queue;
    ClotoSlave* slaves;
    uint32_t slave_count;
} ClotoSlaveGroup;

bool        cloto_slavegroup_create ( ClotoSlaveGroup* group, uint32_t slave_count, uint32_t queue_capacity );
bool        cloto_slavegroup_destroy ( ClotoSlaveGroup* group );
void        cloto_slavegroup_join ( ClotoSlaveGroup* group );
void        cloto_slavegroup_reset ( ClotoSlaveGroup* group );

//--------------------------------------------------------------------------------------------------
// Worker (job fetch-execute-dispatch thread)
// Each worker in a workgroup owns one work queue and can steal fron the other members of the group.
// When creating a workgroup, the calling thread is somewhat special in the sense that it needs to be a simple thread,
// e.g. the main thread or a thread created through cloto_thread_create().
// After the call, that thread becomes a worker itself, pushes the first job and remains trapped inside
// workgroup_create, acting like any other worker thread, until its stop flag is set.

typedef struct ClotoWorkgroup ClotoWorkgroup;
typedef struct {
    ClotoThread thread;
    ClotoWorkgroup* group;
    uint32_t id;
    uint32_t stop_flag;
} ClotoWorker;

bool        cloto_worker_create ( ClotoWorker* worker, ClotoWorkgroup* group, uint32_t id, uint32_t queue_cap );
bool        cloto_worker_join ( ClotoWorker* worker );
bool        cloto_worker_destroy ( ClotoWorker* worker );

typedef struct ClotoWorkgroup {
    ClotoWorker* workers;
    ClotoWorkQueue* queues;
    uint32_t worker_count;
} ClotoWorkgroup;

bool        cloto_workgroup_create ( ClotoWorkgroup* group, uint32_t worker_count, uint32_t queues_capacity, const ClotoJob* seed_job );
bool        cloto_workgroup_destroy ( ClotoWorkgroup* group );

#ifdef __cplusplus
}
#endif

#endif

//--------------------------------------------------------------------------------------------------
// _____                 _                           _        _   _
//|_   _|               | |                         | |      | | (_)
//  | |  _ __ ___  _ __ | | ___ _ __ ___   ___ _ __ | |_ __ _| |_ _  ___  _ __
//  | | | '_ ` _ \| '_ \| |/ _ \ '_ ` _ \ / _ \ '_ \| __/ _` | __| |/ _ \| '_ \ 
// _| |_| | | | | | |_) | |  __/ | | | | |  __/ | | | || (_| | |_| | (_) | | | |
//|_____|_| |_| |_| .__/|_|\___|_| |_| |_|\___|_| |_|\__\__,_|\__|_|\___/|_| |_|
//                | |
//                |_|
//--------------------------------------------------------------------------------------------------

#ifdef CLOTO_IMPLEMENTATION

//--------------------------------------------------------------------------------------------------
// Implementation-local header
//--------------------------------------------------------------------------------------------------
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ClotoThread thread;
    void* group;
    uint32_t id;
    uint32_t stop_flag;
} IClotoMainThread;

#define CLOTO_SAFECALL(call) { bool __retval = call; if (!__retval) return __retval; }

#ifdef _MSC_VER
__declspec ( thread ) ClotoThread* __cloto_thread;
#else
// TODO
#error "Define __declspec(thread) for this compiler!"
#endif

bool cloto_thread_process_messages ( ClotoThread* thread );
void cloto_msgqueue_inc_processed ( ClotoMessageQueue* queue );

//--------------------------------------------------------------------------------------------------
// Malloc
//--------------------------------------------------------------------------------------------------
#ifndef CLOTO_MALLOC

void* cloto_malloc ( size_t size ) {
    return malloc ( size );
}

void* cloto_realloc ( void* p, size_t size ) {
    return realloc ( p, size );
}

void cloto_free ( void* ptr ) {
    free ( ptr );
}

#endif

//--------------------------------------------------------------------------------------------------
// Atomics
//--------------------------------------------------------------------------------------------------
void cloto_memory_barrier() {
#ifdef _WIN32
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

void cloto_compiler_barrier() {
#ifdef _WIN32
    _ReadWriteBarrier();
#else
    asm volatile ( "" ::: "memory" );
#endif
}

#ifndef _WIN32  // TODO DELET THIS
pthread_mutex_t g_mutex;
void cloto_init() {
    pthread_mutex_init ( &g_mutex, NULL );
}
#endif

uint32_t cloto_atomic_fetch_add_u32 ( uint32_t* atomic, uint32_t value ) {
#ifdef _WIN32
    return InterlockedAdd ( ( long* ) atomic, value );
#else   // TODO DELET THIS
    pthread_mutex_lock ( &g_mutex );
    ClotoAtomic32 res = *atomic + value;
    *atomic = res;
    pthread_mutex_unlock ( &g_mutex );
    return res;
    //return __sync_add_and_fetch((long*) atomic, value);
#endif
}

bool cloto_atomic_compare_exchange_u32 ( uint32_t* atomic, uint32_t* expected_read, uint32_t conditional_write ) {
#ifdef _WIN32
    uint32_t read = InterlockedCompareExchange ( atomic, conditional_write, *expected_read );
    bool success = read == *expected_read;
    *expected_read = read;
    return success;
#else   // TODO DELET THIS
    pthread_mutex_lock ( &g_mutex );
    ClotoAtomic32 res = *atomic;

    if ( *atomic == expected ) {
        *atomic = desired;
    }

    pthread_mutex_unlock ( &g_mutex );
    return res;
    //return __sync_val_compare_and_swap((long*) atomic, expected, desired);
#endif
}

//--------------------------------------------------------------------------------------------------
// Message Queue
//--------------------------------------------------------------------------------------------------
bool cloto_msgqueue_create ( ClotoMessageQueue* queue, uint32_t capacity ) {
    if ( ( capacity & ( capacity - 1 ) ) != 0 ) {
        return false;
    }

    queue->messages = ( ClotoMessage* ) cloto_malloc ( sizeof ( ClotoMessage ) * capacity );

    if ( queue->messages == NULL ) {
        return false;
    }

    queue->mask = capacity - 1;
    queue->top = 0;
    queue->bottom = 0;
    queue->processed = 0;

    for ( uint32_t i = 0; i < capacity; ++i ) {
        queue->messages[i].queue_local_id = i;
    }

    return true;
}

void cloto_msgqueue_inc_processed ( ClotoMessageQueue* queue ) {
    ++queue->processed;
}

void cloto_msgqueue_destroy ( ClotoMessageQueue* queue ) {
    cloto_free ( queue->messages );
    queue->messages = NULL;
}

bool cloto_msgqueue_push ( ClotoMessageQueue* queue, const void* data, uint32_t data_size, ClotoMessageType type, uint32_t* msg_id ) {
    uint32_t top = queue->top;
    ClotoMessage* msg = &queue->messages[top & queue->mask];
    uint32_t id = msg->queue_local_id;

    if ( id == top ) {
        if ( cloto_atomic_compare_exchange_u32 ( &queue->top, &top, top + 1 ) ) {
            memcpy ( msg->data, data, data_size );
            msg->size = data_size;
            msg->type = type;
            cloto_compiler_barrier();
            msg->queue_local_id = top + 1;

            if ( msg_id != NULL ) {
                *msg_id = top;
            }

            return true;
        }
    }

    return false;
}

bool cloto_msgqueue_pop ( ClotoMessageQueue* queue, ClotoMessage* dest ) {
    ClotoMessage* msg = &queue->messages[queue->bottom & queue->mask];
    uint32_t id = msg->queue_local_id;
    size_t size = msg->size;
    uint32_t bottom = queue->bottom;

    if ( id == bottom + 1 ) {
        queue->bottom = bottom + 1;
        memcpy ( dest->data, msg->data, size );
        dest->type = msg->type;
        dest->size = msg->size;
        dest->queue_local_id = id;
        cloto_compiler_barrier();
        msg->queue_local_id = bottom + queue->mask + 1;
        return true;
    }

    return false;
}

bool cloto_usermsgqueue_create ( ClotoUserMessageQueue* queue, uint32_t capacity ) {
    if ( ( capacity & ( capacity - 1 ) ) != 0 ) {
        return false;
    }

    queue->messages = ( ClotoUserMessage* ) cloto_malloc ( sizeof ( ClotoUserMessage ) * capacity );
    queue->mask = capacity - 1;
    queue->bottom = 0;
    queue->top = 0;
    return true;
}

void cloto_usermsgqueue_destroy ( ClotoUserMessageQueue* queue ) {
    cloto_free ( queue->messages );
}

bool cloto_usermsgqueue_push ( ClotoUserMessageQueue* queue, const ClotoMessage* msg ) {
    if ( queue->top - queue->bottom == queue->mask + 1 ) {
        return false;
    }

    memcpy ( queue->messages[queue->top].data, msg->data, msg->size );
    queue->messages[queue->top].data_size = msg->size;
    ++queue->top;
    return true;
}

bool cloto_usermsgqueue_pop ( ClotoUserMessageQueue* queue, ClotoUserMessage* msg ) {
    if ( queue->top == queue->bottom ) {
        return false;
    }

    *msg = queue->messages[queue->bottom++];
    return true;
}

//--------------------------------------------------------------------------------------------------
// Job
//--------------------------------------------------------------------------------------------------
void cloto_job_create ( ClotoJob* job, ClotoJobRoutine* routine, void* args ) {
    job->routine = routine;
    job->args = args;
}

//--------------------------------------------------------------------------------------------------
// Work Queue
//--------------------------------------------------------------------------------------------------
bool cloto_workqueue_create ( ClotoWorkQueue* queue, uint32_t capacity ) {
    if ( ( capacity & ( capacity - 1 ) ) != 0 ) {
        return false;
    }

    queue->jobs = ( ClotoJob* ) cloto_malloc ( sizeof ( ClotoJob ) * capacity );

    if ( queue->jobs == NULL ) {
        return false;
    }

    queue->top = 0;
    queue->bottom = 0;
    queue->mask = capacity - 1;
    return true;
}

void cloto_workqueue_destroy ( ClotoWorkQueue* queue ) {
    cloto_free ( queue->jobs );
    queue->jobs = NULL;
}

// A compiler fence is necessary to properly place the publish always after the item write inside the queue.
bool cloto_workqueue_push ( ClotoWorkQueue* queue, const ClotoJob* job ) {
    uint32_t bottom = queue->bottom;
    uint32_t top = queue->top;

    // Do a free size check. The order of read top/bottom is irrelevant, as nobody else is touching top.
    if ( top - bottom >= queue->mask + 1 ) {
        return false;
    }

    queue->jobs[top & queue->mask] = *job;
    cloto_compiler_barrier();
    cloto_atomic_fetch_add_u32 ( &queue->top, 1 );
    return true;
}

/*
    Case 1:
        [pop]   --top
        [steal] read top
    Case 2:
        [steal] read top
        [pop]   --top
        [steal] ++bottom
        [pop]   read bottom
    Case 3:
        [steal] read top
        [pop]   --top
        [pop]   read bottom
        [steal] ++bottom

    Case 1 is fine. The pop operation ends with the decrement and the following steal has full knowledge.
    Case 2 is also fine, as the pop has full knowledge and can react properly.
        bottom < top: fine.
        bottom > top: the steal took the item, so the pop acts accordingly and fails.
        bottom = top: fine. (queue is empty)
    Case 3 can be problematic, as this time the pop does not know about the ongoing steal.
        bottom < top: fine.
        bottom > top: fail
        bottom = top: in this case the ongoing steal is about to (or has already) increase bottom, so it is not fine.
            To solve this problem, the pop shall also do a cas on bottom, using its own value.
            This way, either the steal run first and get the item, or the pop does run first and invalidate
            the bottom read from the steal, thus forcing the steal to another new full run.

    For the above to be true, a few enforcements have to be placed on the order of the instructions of individual
    execution agents.
        1. A steal should read bottom always first, so that the '[steal] read top' event always implies that
            the read to bottom had already happened. This is a compiler fence.
        2. A pop decreases top always first, and then reads bottom. This is a memory fence.

*/
bool cloto_workqueue_pop ( ClotoWorkQueue* queue, ClotoJob* dest ) {
    uint32_t top = queue->top - 1;
    queue->top = top;
    cloto_memory_barrier();
    uint32_t bottom = queue->bottom;

    if ( bottom > top ) {
        queue->top = bottom;
        return false;
    }

    ClotoJob job = queue->jobs[top & queue->mask];

    if ( bottom < top ) {
        *dest = job;
        return true;
    }

    // bottom == top
    queue->top = bottom + 1;

    if ( cloto_atomic_compare_exchange_u32 ( &queue->bottom, &bottom, bottom + 1 ) ) {
        *dest = job;
        return true;
    }

    return false;
}

bool cloto_workqueue_steal ( ClotoWorkQueue* queue, ClotoJob* job ) {
    uint32_t bottom = queue->bottom;
    cloto_compiler_barrier();
    uint32_t top = queue->top;

    if ( bottom >= top ) {
        return false;
    }

    ClotoJob _job = queue->jobs[bottom & queue->mask];

    if ( cloto_atomic_compare_exchange_u32 ( &queue->bottom, &bottom, bottom + 1 ) ) {
        *job = _job;
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// Task Queue
//--------------------------------------------------------------------------------------------------

// TODO

//--------------------------------------------------------------------------------------------------
// Threading API
//--------------------------------------------------------------------------------------------------
#ifdef _WIN32

void cloto_thread_register() {
    IClotoMainThread* i = ( IClotoMainThread* ) cloto_malloc ( sizeof ( IClotoMainThread ) );
    i->thread.routine = NULL;
    i->thread.args = NULL;
    i->thread.handle = GetCurrentThread();
    cloto_msgqueue_create ( &i->thread.msg_queue, 32 );
    cloto_usermsgqueue_create ( &i->thread.user_msg_queue, 32 );
    i->group = NULL;
    i->id = 0;
    i->stop_flag = 0;
    __cloto_thread = &i->thread;
}

void cloto_thread_dispose() {
    cloto_msgqueue_destroy ( &__cloto_thread->msg_queue );
    cloto_usermsgqueue_destroy ( &__cloto_thread->user_msg_queue );
    cloto_free ( ( IClotoMainThread* ) __cloto_thread );
}

DWORD WINAPI cloto_thread_launcher ( LPVOID param ) {
    ClotoThread* thread = ( ClotoThread* ) param;
    __cloto_thread = thread;
    thread->routine ( thread->args );
    return 0;
}

bool cloto_thread_create ( ClotoThread* thread, ClotoThreadRoutine* routine, void* args, uint32_t msg_queue_cap, uint32_t user_msg_queue_cap ) {
    HANDLE handle = CreateThread ( NULL, 0, cloto_thread_launcher, thread, 0, NULL );

    if ( handle == NULL ) {
        return false;
    }

    thread->handle = handle;
    thread->routine = routine;
    thread->args = args;
    CLOTO_SAFECALL ( cloto_msgqueue_create ( &thread->msg_queue, msg_queue_cap ) );
    CLOTO_SAFECALL ( cloto_usermsgqueue_create ( &thread->user_msg_queue, user_msg_queue_cap ) );
    return true;
}

bool cloto_thread_join ( ClotoThread* thread ) {
    DWORD result = WaitForSingleObject ( thread->handle, INFINITE );
    return result == WAIT_OBJECT_0;
}

bool cloto_thread_destroy ( ClotoThread* thread ) {
    BOOL result = CloseHandle ( thread->handle );

    if ( result == 0 ) {
        return false;
    }

    thread->handle = 0;
    cloto_msgqueue_destroy ( &thread->msg_queue );
    cloto_usermsgqueue_destroy ( &thread->user_msg_queue );
    return true;
}

void cloto_thread_yield() {
    SwitchToThread();
}

#else

void* cloto_thread_launcher ( void* param ) {
    ClotoThread* thread = ( ClotoThread* ) param;
    thread->routine ( thread->args );
    return 0;
}

bool cloto_thread_create ( ClotoThread* thread, ClotoThreadRoutine routine, void* args ) {
    thread->routine = routine;
    thread->args = args;
    pthread_t handle;
    int result = pthread_create ( &handle, NULL, cloto_thread_launcher, thread );

    if ( result == 0 ) {
        thread->handle = handle;
        return true;
    }

    return false;
}

bool cloto_thread_join ( ClotoThread* thread ) {
    int result = pthread_join ( thread->handle, NULL );

    if ( result == 0 ) {
        thread->handle = 0;
        return true;
    }

    return false;
}

bool cloto_thread_destroy ( ClotoThread* thread ) {
    // pthread has no destroy, only join.
    return thread->handle == 0;
}

void cloto_thread_yield() {
    pthread_yield();
}

#endif

ClotoThread* cloto_thread_get() {
    return __cloto_thread;
}

bool cloto_thread_process_messages ( ClotoThread* thread ) {
    if ( thread->msg_queue.bottom == thread->msg_queue.top ) {
        return true;
    }

    CLOTO_DECL_ALIGN ( 64 ) ClotoMessage msg;

    while ( thread->msg_queue.bottom != thread->msg_queue.top ) {
        if ( cloto_msgqueue_pop ( &thread->msg_queue, &msg ) ) {
            switch ( msg.type ) {
                case CLOTO_MSG_JOB_NO_ARGS: {
                    ClotoJob* j = ( ClotoJob* ) &msg;
                    j->routine ( NULL );
                }
                break;

                case CLOTO_MSG_JOB_REMOTE_ARGS: {
                    ClotoJob* j = ( ClotoJob* ) &msg;
                    j->routine ( j->args );
                }
                break;

                case CLOTO_MSG_JOB_LOCAL_ARGS: {
                    ClotoJob* j = ( ClotoJob* ) &msg;
                    j->routine ( &j->args );
                }
                break;

                case CLOTO_MSG_USER:
                    CLOTO_SAFECALL ( cloto_usermsgqueue_push ( &thread->user_msg_queue, &msg ) );
                    break;

                default:
                    assert ( false );
            }

            cloto_msgqueue_inc_processed ( &thread->msg_queue );
        }
    }

    return true;
}

uint32_t cloto_thread_send_message ( ClotoThread* to, const void* data, size_t size, ClotoMessageType type ) {
    uint32_t id;

    while ( !cloto_msgqueue_push ( &to->msg_queue, data, ( uint32_t ) size, type, &id ) ) {
        cloto_thread_yield();
    }

    return id;
}

bool cloto_thread_query_message_status ( ClotoThread* destinatary, uint32_t msg_id ) {
    return destinatary->msg_queue.processed > msg_id;
}

//--------------------------------------------------------------------------------------------------
// Slaves
//--------------------------------------------------------------------------------------------------
void cloto_slave_thread_routine ( void* args ) {
    ClotoSlave* self = ( ClotoSlave* ) args;
    ClotoJob job;

    while ( !self->stop_flag ) {
        if ( !cloto_thread_process_messages ( &self->thread ) ) {
            // TODO something went wrong (full user msg queue)
        }

        if ( cloto_workqueue_steal ( &self->group->queue, &job ) ) {
            job.routine ( job.args );
        } else {
            cloto_thread_yield();
        }
    }
}

bool cloto_slave_create ( ClotoSlave* slave, ClotoSlaveGroup* group, uint32_t id ) {
    slave->group = group;
    slave->id = id;
    slave->stop_flag = 0;
    CLOTO_SAFECALL ( cloto_thread_create ( &slave->thread, &cloto_slave_thread_routine, slave, 32, 32 ) );
    return true;
}

bool cloto_slave_join ( ClotoSlave* slave ) {
    slave->stop_flag = 1;
    CLOTO_SAFECALL ( cloto_thread_join ( &slave->thread ) );
    return true;
}

bool cloto_slave_destroy ( ClotoSlave* slave ) {
    CLOTO_SAFECALL ( cloto_thread_destroy ( &slave->thread ) );
    return true;
}

bool cloto_slavegroup_create ( ClotoSlaveGroup* group, uint32_t slave_count, uint32_t queue_cap ) {
    group->slaves = ( ClotoSlave* ) cloto_malloc ( sizeof ( ClotoSlave ) * slave_count );
    group->slave_count = slave_count;
    CLOTO_SAFECALL ( cloto_workqueue_create ( &group->queue, queue_cap ) );

    for ( uint32_t i = 0; i < group->slave_count; ++i ) {
        CLOTO_SAFECALL ( cloto_slave_create ( &group->slaves[i], group, i ) );
    }

    return true;
}

bool cloto_slavegroup_destroy ( ClotoSlaveGroup* group ) {
    // Better set all flags before waiting for any thread to join
    for ( size_t i = 0; i < group->slave_count; ++i ) {
        group->slaves[i].stop_flag = 1;
    }

    for ( size_t i = 0; i < group->slave_count; ++i ) {
        CLOTO_SAFECALL ( cloto_slave_join ( &group->slaves[i] ) )
        CLOTO_SAFECALL ( cloto_slave_destroy ( &group->slaves[i] ) )
    }

    cloto_workqueue_destroy ( &group->queue );
    group->slave_count = 0;
    return true;
}

void cloto_slavegroup_join ( ClotoSlaveGroup* group ) {
    ClotoJob job;
    ClotoThread* thread = cloto_thread_get();

    while ( group->queue.bottom != group->queue.top ) {
        if ( !cloto_thread_process_messages ( thread ) ) {
            // TODO full user msg queue
        }

        if ( cloto_workqueue_steal ( &group->queue, &job ) ) {
            job.routine ( job.args );
        } else {
            cloto_thread_yield();
        }
    }
}

void cloto_slavegroup_reset ( ClotoSlaveGroup* group ) {
    group->queue.bottom = 0;
}

//--------------------------------------------------------------------------------------------------
// Workers
//--------------------------------------------------------------------------------------------------
void cloto_worker_thread_routine ( void* args ) {
    ClotoWorker* self = ( ClotoWorker* ) args;
    ClotoWorkQueue* queue = &self->group->queues[self->id];
    ClotoJob job;

    while ( !self->stop_flag ) {
        if ( !cloto_thread_process_messages ( &self->thread ) ) {
            // TODO full msg queue
        }

        if ( cloto_workqueue_pop ( queue, &job ) ) {
            job.routine ( job.args );
        } else {
            bool job_found = false;

            for ( size_t i = 0; i < self->group->worker_count; ++i ) {
                if ( i == self->id ) {
                    continue;
                }

                if ( cloto_workqueue_steal ( &self->group->queues[i], &job ) ) {
                    job_found = true;
                    job.routine ( job.args );
                }
            }

            if ( !job_found ) {
                cloto_thread_yield();
            }
        }
    }
}

bool cloto_worker_create ( ClotoWorker* worker, ClotoWorkgroup* group, uint32_t id, uint32_t queue_cap ) {
    CLOTO_SAFECALL ( cloto_workqueue_create ( &group->queues[id], queue_cap ) );
    worker->id = id;
    worker->stop_flag = 0;
    worker->group = group;
    CLOTO_SAFECALL ( cloto_thread_create ( &worker->thread, &cloto_worker_thread_routine, worker, 32, 32 ) );
    return true;
}

bool cloto_worker_join ( ClotoWorker* worker ) {
    worker->stop_flag = 1;
    return cloto_thread_join ( &worker->thread );
}

bool cloto_worker_destroy ( ClotoWorker* worker ) {
    return cloto_thread_destroy ( &worker->thread );
}

bool cloto_workgroup_create ( ClotoWorkgroup* group, uint32_t worker_count, uint32_t queues_cap, const ClotoJob* job ) {
    group->workers = ( ClotoWorker* ) cloto_malloc ( sizeof ( ClotoWorker ) * ( worker_count + 1 ) );
    group->worker_count = 0;

    for ( uint32_t i = 0; i < group->worker_count; ++i ) {
        CLOTO_SAFECALL ( cloto_worker_create ( &group->workers[i], group, i, queues_cap ) );
        ++group->worker_count;
    }

    ClotoWorker* self = ( ClotoWorker* ) cloto_thread_get();
    self->group = group;
    self->id = worker_count;
    self->stop_flag = 0;
    CLOTO_SAFECALL ( cloto_workqueue_create ( &group->queues[worker_count], queues_cap ) );
    cloto_workqueue_push ( &group->queues[worker_count], job );
    ++group->worker_count;  // push before entering group
    cloto_worker_thread_routine ( self );
    return true;
}

bool cloto_workgroup_destroy ( ClotoWorkgroup* group ) {
    for ( size_t i = 0; i < group->worker_count; ++i ) {
        group->workers[i].stop_flag = 1;
    }

    for ( size_t i = 0; i < group->worker_count; ++i ) {
        CLOTO_SAFECALL ( cloto_worker_join ( &group->workers[i] ) );
        CLOTO_SAFECALL ( cloto_worker_destroy ( &group->workers[i] ) );
    }

    return true;
}

#ifdef __cplusplus
}
#endif

#endif