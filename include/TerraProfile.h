#pragma once

// Terra
#include <Terra.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef int64_t TerraClockTime;
#else
typedef clock_t TerraClockTime;
#endif

TerraClockTime      terra_clock();
double              terra_clock_to_ms ( TerraClockTime delta_time );
double              terra_clock_to_us ( TerraClockTime delta_time );

#ifdef TERRA_PROFILE

// Max sessions/targets supported
#define TERRA_PROFILE_SESSIONS             8
#define TERRA_PROFILE_TARGETS_PER_SESSION  128

// Threads that want to collect samples have to register before starting to collect.
// Collected data is grouped in sessions and targets. Most situations should only require one session.
// Targets are the entities to be profiled. For each target, a data collection buffer is created for each thread.
// Currently the data buffer gets flushed on every stats update. This greatly helps in cases with very high sample collection frequency.
// On the other hand, cool things like filtering off samples subsets based on e.g. time or outlier values become impossible.

// Stats are always computed in double floating precision.
typedef struct {
    double avg;
    double var;
    double min;
    double max;
    double sum;
    double n;
} TerraProfileStats;

typedef __declspec ( align ( 64 ) ) struct {
    // Thread local buffer stats.
    TerraProfileStats stats;
    // Buffer data.
    void* value;
    size_t size;    // Used size
    size_t cap;     // Total size
} TerraProfileBuffer;

typedef enum {
    U32,
    U64,
    I32,
    I64,
    F32,
    F64,
    TIME,
} TerraProfileSampleType;

typedef struct {
    // Sample type.
    TerraProfileSampleType type;
    // Global stats. They are computed putting together the sample buffers.
    TerraProfileStats stats;
    // One buffer per thread.
    TerraProfileBuffer* buffers;
} TerraProfileTarget;

typedef struct {
    size_t threads;
    uint32_t registered_threads;
    TerraProfileTarget* targets;
} TerraProfileSession;

typedef struct {
    TerraProfileSession* sessions;
} TerraProfileDatabase;

extern TerraProfileDatabase g_terra_profile_database;

void                    terra_profile_session_create ( size_t id, size_t threads );
void                    terra_profile_register_thread ( size_t session );
void                    terra_profile_session_delete ( size_t id );

void                    terra_profile_target_create_u32 ( size_t session, size_t target, size_t sample_cap );
void                    terra_profile_target_create_u64 ( size_t session, size_t target, size_t sample_cap );
void                    terra_profile_target_create_i32 ( size_t session, size_t target, size_t sample_cap );
void                    terra_profile_target_create_i64 ( size_t session, size_t target, size_t sample_cap );
void                    terra_profile_target_create_f32 ( size_t session, size_t target, size_t sample_cap );
void                    terra_profile_target_create_f64 ( size_t session, size_t target, size_t sample_cap );
void                    terra_profile_target_create_time ( size_t session, size_t target, size_t sample_cap );


void                    terra_profile_target_clear ( size_t session, size_t target );
void                    terra_profile_target_stats_update ( size_t session, size_t target );
void                    terra_profile_target_local_stats_update ( size_t session, size_t target );

void                    terra_profile_add_sample_u32 ( size_t session, size_t target, uint32_t value );
void                    terra_profile_add_sample_u64 ( size_t session, size_t target, uint64_t value );
void                    terra_profile_add_sample_i32 ( size_t session, size_t target, int32_t value );
void                    terra_profile_add_sample_i64 ( size_t session, size_t target, int64_t value );
void                    terra_profile_add_sample_f32 ( size_t session, size_t target, float value );
void                    terra_profile_add_sample_f64 ( size_t session, size_t target, double value );
void                    terra_profile_add_sample_time ( size_t session, size_t target, TerraClockTime value );

size_t                  terra_profile_target_size ( size_t session, size_t target );
size_t                  terra_profile_target_local_size ( size_t session, size_t target );
TerraProfileStats       terra_profile_target_stats_get ( size_t session, size_t target );
TerraProfileStats       terra_profile_target_local_stats_get ( size_t session, size_t target );
TerraProfileSampleType  terra_profile_target_type_get ( size_t session, size_t target );
bool                    terra_profile_session_exists ( size_t session );


// Use these macros to wrap profile calls so that when necessary they can be easily turned off without having to edit out code.
// Stats getters are not wrapped since TerraProfile types are generally part of them anyway.
#define TERRA_PROFILE_CREATE_SESSION( session, threads )                    terra_profile_session_create ( session, threads )
#define TERRA_PROFILE_REGISTER_THREAD( session )                            terra_profile_register_thread (session )
#define TERRA_PROFILE_CREATE_TARGET( format, session, target, sample_cap)   terra_profile_target_create_ ## format ( session, target, sample_cap )
#define TERRA_PROFILE_ADD_SAMPLE( format, session, target, value)           terra_profile_add_sample_ ## format ( session, target, value )
#define TERRA_PROFILE_UPDATE_STATS( session, target )                       terra_profile_target_stats_update ( session, target )
#define TERRA_PROFILE_UPDATE_LOCAL_STATS( session, target )                 terra_profile_target_local_stats_update ( session, target )
#define TERRA_PROFILE_CLEAR_TARGET( session, target )                       terra_profile_target_clear ( session, target )
#define TERRA_PROFILE_DELETE_SESSION( session )                             terra_profile_session_delete ( session )
#define TERRA_CLOCK()                                                       terra_clock()

#else

#define TERRA_PROFILE_REGISTER_THREAD( session )                            0
#define TERRA_PROFILE_CREATE_SESSION( session, threads )                    0
#define TERRA_PROFILE_CREATE_TARGET( format, session, target, sample_cap)   0
#define TERRA_PROFILE_ADD_SAMPLE( format, session, target, value)           0
#define TERRA_PROFILE_UPDATE_STATS( session, target )                       0
#define TERRA_PROFILE_UPDATE_LOCAL_STATS( session, target )                 0
#define TERRA_PROFILE_CLEAR_TARGET( session, target )                       0
#define TERRA_PROFILE_DELETE_SESSION( session, target )                     0
#define TERRA_CLOCK()                                                       0

#endif

#ifdef __cplusplus
}
#endif
