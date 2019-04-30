// TerraProfile
#include "TerraProfile.h"

// Terra
#include "Terra.h"

// libc
#include <string.h>
#include <assert.h>

// .c env

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TERRA_PROFILE

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <Windows.h>
#define ATOMIC_ADD InterlockedIncrement
__declspec ( thread ) uint32_t t_terra_profile_thread_id;
#else
// TODO
#error
#endif

#define TERRA_PID t_terra_profile_thread_id

TerraProfileDatabase g_terra_profile_database;
#define TERRA_PDB g_terra_profile_database

// .c decl

size_t terra_profile_sizeof ( TerraProfileSampleType type );

void terra_profile_stats_init ( TerraProfileStats* stats );
TerraProfileStats terra_profile_stats_combine ( TerraProfileStats* s1, TerraProfileStats* s2 );

void terra_profile_update_stats_u32 ( size_t session, size_t target );
void terra_profile_update_stats_u64 ( size_t session, size_t target );
void terra_profile_update_stats_i32 ( size_t session, size_t target );
void terra_profile_update_stats_i64 ( size_t session, size_t target );
void terra_profile_update_stats_f32 ( size_t session, size_t target );
void terra_profile_update_stats_f64 ( size_t session, size_t target );
void terra_profile_update_stats_time ( size_t session, size_t target );

void terra_profile_update_local_stats_u32 ( size_t session, size_t target );
void terra_profile_update_local_stats_u64 ( size_t session, size_t target );
void terra_profile_update_local_stats_i32 ( size_t session, size_t target );
void terra_profile_update_local_stats_i64 ( size_t session, size_t target );
void terra_profile_update_local_stats_f32 ( size_t session, size_t target );
void terra_profile_update_local_stats_f64 ( size_t session, size_t target );
void terra_profile_update_local_stats_time ( size_t session, size_t target );

void terra_profile_update_thread_stats_u32 ( size_t session, size_t target );
void terra_profile_update_thread_stats_u64 ( size_t session, size_t target );
void terra_profile_update_thread_stats_i32 ( size_t session, size_t target );
void terra_profile_update_thread_stats_i64 ( size_t session, size_t target );
void terra_profile_update_thread_stats_f32 ( size_t session, size_t target );
void terra_profile_update_thread_stats_f64 ( size_t session, size_t target );
void terra_profile_update_thread_stats_time ( size_t session, size_t target );

// .h defs

void terra_profile_session_create ( size_t id, size_t threads ) {
    if ( TERRA_PDB.sessions == NULL ) {
        TERRA_PDB.sessions = terra_malloc ( sizeof ( TerraProfileSession ) * TERRA_PROFILE_SESSIONS );
    }

    TerraProfileSession* session = TERRA_PDB.sessions + id;
    session->threads = threads;
    session->registered_threads = 0;
    session->targets = terra_malloc ( sizeof ( TerraProfileTarget ) * TERRA_PROFILE_TARGETS_PER_SESSION );

    for ( size_t i = 0; i < TERRA_PROFILE_TARGETS_PER_SESSION; ++i ) {
        session->targets[i].buffers = NULL;
    }
}

void terra_profile_register_thread ( size_t session ) {
    TERRA_PID = ATOMIC_ADD ( &TERRA_PDB.sessions[session].registered_threads ) - 1;
}

void terra_profile_session_delete ( size_t id ) {
    TerraProfileSession* session = TERRA_PDB.sessions + id;

    for ( size_t i = 0; i < TERRA_PROFILE_TARGETS_PER_SESSION; ++i ) {
        TerraProfileTarget* target = session->targets + i;

        if ( target->buffers == NULL ) {
            continue;
        }

        for ( size_t j = 0; j < session->threads; ++j ) {
            terra_free ( target->buffers[j].time );
            terra_free ( target->buffers[j].value );
        }
    }
}

void terra_profile_target_clear ( size_t _session, size_t _target ) {
    TerraProfileSession* session = TERRA_PDB.sessions + _session;

    if ( !session ) {
        return;
    }

    TerraProfileTarget* target = session->targets + _target;

    for ( size_t i = 0; i < session->threads; ++i ) {
        terra_profile_stats_init ( &target->buffers[i].stats );
        target->buffers[i].size = 0;
    }

    terra_profile_stats_init ( &target->stats );
}

size_t terra_profile_target_size ( size_t session, size_t target ) {
    size_t n = 0;

    for ( size_t i = 0; i < TERRA_PDB.sessions[session].registered_threads; ++i ) {
        n += TERRA_PDB.sessions[session].targets[target].buffers[TERRA_PID].size;
    }

    return n;
}

size_t terra_profile_target_local_size ( size_t session, size_t target ) {
    return TERRA_PDB.sessions[session].targets[target].buffers[TERRA_PID].size;
}

void terra_profile_target_stats_update ( size_t session, size_t target ) {
    switch ( TERRA_PDB.sessions[session].targets[target].type ) {
        case U32:
            terra_profile_update_stats_u32 ( session, target );
            break;

        case U64:
            terra_profile_update_stats_u64 ( session, target );
            break;

        case I32:
            terra_profile_update_stats_i32 ( session, target );
            break;

        case I64:
            terra_profile_update_stats_i64 ( session, target );
            break;

        case F32:
            terra_profile_update_stats_f32 ( session, target );
            break;

        case F64:
            terra_profile_update_stats_f64 ( session, target );
            break;

        case TIME:
            terra_profile_update_stats_time ( session, target );
            break;
    }
}

void terra_profile_target_local_stats_update ( size_t session, size_t target ) {
    switch ( TERRA_PDB.sessions[session].targets[target].type ) {
        case U32:
            terra_profile_update_local_stats_u32 ( session, target );
            break;

        case U64:
            terra_profile_update_local_stats_u64 ( session, target );
            break;

        case I32:
            terra_profile_update_local_stats_i32 ( session, target );
            break;

        case I64:
            terra_profile_update_local_stats_i64 ( session, target );
            break;

        case F32:
            terra_profile_update_local_stats_f32 ( session, target );
            break;

        case F64:
            terra_profile_update_local_stats_f64 ( session, target );
            break;

        case TIME:
            terra_profile_update_local_stats_time ( session, target );
            break;
    }
}

TerraProfileStats terra_profile_target_stats_get ( size_t session, size_t target ) {
    return TERRA_PDB.sessions[session].targets[target].stats;
}

TerraProfileStats terra_profile_target_local_stats_get ( size_t session, size_t target ) {
    return TERRA_PDB.sessions[session].targets[target].buffers[TERRA_PID].stats;
}

TerraProfileSampleType terra_profile_target_type_get ( size_t session, size_t target ) {
    return TERRA_PDB.sessions[session].targets[target].type;
}

// .c defs

size_t terra_profile_sizeof ( TerraProfileSampleType type ) {
    switch ( type ) {
        case U32:
            return sizeof ( uint32_t );

        case U64:
            return sizeof ( uint64_t );

        case I32:
            return sizeof ( int32_t );

        case I64:
            return sizeof ( int64_t );

        case F32:
            return sizeof ( float );

        case F64:
            return sizeof ( double );

        case TIME:
            return sizeof ( TerraClockTime );
    }
}

void terra_profile_stats_init ( TerraProfileStats* stats ) {
    stats->n = 0;
    stats->sum = 0;
    stats->avg = 0;
    stats->var = 0;
    stats->min = DBL_MAX;
    stats->max = -DBL_MAX;
}

TerraProfileStats terra_profile_stats_combine ( TerraProfileStats* s1, TerraProfileStats* s2 ) {
    TerraProfileStats s;
    s.avg = ( s1->sum + s2->sum ) / ( s1->n + s2->n );
    double d1 = s1->avg - s.avg;
    double d2 = s2->avg - s.avg;
    s.var = ( s1->n * ( s1->var + d1 * d1 ) + s2->n * ( s2->var + d2 * d2 ) ) / ( s1->n + s2->n );
    s.min = s1->min < s2->min ? s1->min : s2->min;
    s.max = s1->max > s2->max ? s1->max : s2->max;
    s.n = s1->n + s2->n;
    s.sum = s1->sum + s2->sum;
    return s;
}

#define TERRA_TYPE_ENUM_u32     U32
#define TERRA_TYPE_ENUM_u64     U64
#define TERRA_TYPE_ENUM_i32     I32
#define TERRA_TYPE_ENUM_i64     I64
#define TERRA_TYPE_ENUM_f32     F32
#define TERRA_TYPE_ENUM_f64     F64
#define TERRA_TYPE_ENUM_time    TIME

#define TERRA_TYPE_TO_ENUM( type ) TERRA_TYPE_ENUM_ ## type

#define TERRA_PROFILE_DEFINE_TARGET_CREATOR( postfix, _type )                                                       \
    void terra_profile_target_create_ ## postfix ( size_t _session, size_t _target, size_t sample_cap ) {           \
        TerraProfileSession* session = TERRA_PDB.sessions + _session;                                               \
        TerraProfileTarget* target = session->targets + _target;                                                    \
        target->type = TERRA_TYPE_TO_ENUM ( postfix );                                                              \
        target->buffers = terra_malloc ( sizeof ( TerraProfileBuffer ) * session->threads );                        \
        terra_profile_stats_init ( &target->stats );                                                                \
        for ( size_t i = 0; i < session->threads; ++i ) {                                                           \
            target->buffers[i].time = ( TerraClockTime* ) terra_malloc( sizeof( TerraClockTime ) * sample_cap );    \
            target->buffers[i].value = terra_malloc ( sizeof( _type ) * sample_cap );                               \
            target->buffers[i].size = 0;                                                                            \
            target->buffers[i].cap = sample_cap;                                                                    \
            terra_profile_stats_init ( &target->buffers[i].stats );                                                 \
        }                                                                                                           \
    }
TERRA_PROFILE_DEFINE_TARGET_CREATOR ( u32, uint32_t )
TERRA_PROFILE_DEFINE_TARGET_CREATOR ( u64, uint64_t )
TERRA_PROFILE_DEFINE_TARGET_CREATOR ( i32, int32_t )
TERRA_PROFILE_DEFINE_TARGET_CREATOR ( i64, int64_t )
TERRA_PROFILE_DEFINE_TARGET_CREATOR ( f32, float )
TERRA_PROFILE_DEFINE_TARGET_CREATOR ( f64, double )
TERRA_PROFILE_DEFINE_TARGET_CREATOR ( time, TerraClockTime )

#define TERRA_PROFILE_DEFINE_COLLECTOR(postfix, type)                                                   \
    void terra_profile_add_sample_ ## postfix ( size_t session, size_t target, type value ) {           \
        TerraClockTime time = terra_clock();                                                            \
        TerraProfileBuffer* buffer = &TERRA_PDB.sessions[session].targets[target].buffers[TERRA_PID];   \
        if (buffer->size == buffer->cap) { return; }                                                    \
        buffer->time[buffer->size] = time;                                                              \
        ( ( type* ) buffer->value ) [buffer->size] = value;                                             \
        ++buffer->size;                                                                                 \
    }
TERRA_PROFILE_DEFINE_COLLECTOR ( u32, uint32_t )
TERRA_PROFILE_DEFINE_COLLECTOR ( u64, uint64_t )
TERRA_PROFILE_DEFINE_COLLECTOR ( i32, int32_t )
TERRA_PROFILE_DEFINE_COLLECTOR ( i64, int64_t )
TERRA_PROFILE_DEFINE_COLLECTOR ( f32, float )
TERRA_PROFILE_DEFINE_COLLECTOR ( f64, double )
TERRA_PROFILE_DEFINE_COLLECTOR ( time, TerraClockTime )

#define TERRA_PROFILE_DEFINE_THREAD_UPDATE( postfix, type )                                                 \
    void terra_profile_update_thread_stats_ ## postfix ( size_t session, size_t target, size_t thread ) {   \
        TerraProfileStats s;                                                                                \
        terra_profile_stats_init ( &s );                                                                    \
        double sum_sq = 0;                                                                                  \
        TerraProfileBuffer* buffer = &TERRA_PDB.sessions[session].targets[target].buffers[thread];          \
        if (buffer->size == 0) return;                                                                      \
        s.n = ( double ) buffer->size;                                                                      \
        for ( size_t i = 0; i < buffer->size; ++i ) {                                                       \
            type v = ( ( type* ) buffer->value ) [i];                                                       \
            s.sum += v;                                                                                     \
            sum_sq += v * v;                                                                                \
            if ( v < s.min ) s.min = v;                                                                     \
            if ( v > s.max ) s.max = v;                                                                     \
        }                                                                                                   \
        s.avg = s.sum / s.n;                                                                                \
        double avg_sq = sum_sq / s.n;                                                                       \
        s.var = avg_sq - s.avg * s.avg;                                                                     \
        buffer->stats = terra_profile_stats_combine ( &s, &buffer->stats );                                 \
        buffer->size = 0;                                                                                   \
    }
TERRA_PROFILE_DEFINE_THREAD_UPDATE ( u32, uint32_t )
TERRA_PROFILE_DEFINE_THREAD_UPDATE ( u64, uint64_t )
TERRA_PROFILE_DEFINE_THREAD_UPDATE ( i32, int32_t )
TERRA_PROFILE_DEFINE_THREAD_UPDATE ( i64, int64_t )
TERRA_PROFILE_DEFINE_THREAD_UPDATE ( f32, float )
TERRA_PROFILE_DEFINE_THREAD_UPDATE ( f64, double )
TERRA_PROFILE_DEFINE_THREAD_UPDATE ( time, TerraClockTime )

#define TERRA_PROFILE_DEFINE_LOCAL_UPDATE( postfix )                                  \
    void terra_profile_update_local_stats_ ## postfix ( size_t session, size_t target ) {   \
        terra_profile_update_thread_stats_ ## postfix ( session, target, TERRA_PID );       \
    }
TERRA_PROFILE_DEFINE_LOCAL_UPDATE ( u32 )
TERRA_PROFILE_DEFINE_LOCAL_UPDATE ( u64 )
TERRA_PROFILE_DEFINE_LOCAL_UPDATE ( i32 )
TERRA_PROFILE_DEFINE_LOCAL_UPDATE ( i64 )
TERRA_PROFILE_DEFINE_LOCAL_UPDATE ( f32 )
TERRA_PROFILE_DEFINE_LOCAL_UPDATE ( f64 )
TERRA_PROFILE_DEFINE_LOCAL_UPDATE ( time )

#define TERRA_PROFILE_DEFINE_UPDATE( postfix )                                                \
    void terra_profile_update_stats_ ## postfix( size_t session, size_t target ) {                  \
        for ( size_t i = 0; i < TERRA_PDB.sessions[session].threads; ++i ) {                        \
            terra_profile_update_thread_stats_ ## postfix ( session, target, i );                   \
        }                                                                                           \
        TerraProfileStats stats;                                                                    \
        terra_profile_stats_init ( &stats );                                                        \
        for ( size_t i = 0; i < TERRA_PDB.sessions[session].threads; ++i ) {                        \
            TerraProfileBuffer* buffer = &TERRA_PDB.sessions[session].targets[target].buffers[i];   \
            stats = terra_profile_stats_combine ( &stats, &buffer->stats );                         \
        }                                                                                           \
        TERRA_PDB.sessions[session].targets[target].stats = stats;                                  \
    }
TERRA_PROFILE_DEFINE_UPDATE ( u32 )
TERRA_PROFILE_DEFINE_UPDATE ( u64 )
TERRA_PROFILE_DEFINE_UPDATE ( i32 )
TERRA_PROFILE_DEFINE_UPDATE ( i64 )
TERRA_PROFILE_DEFINE_UPDATE ( f32 )
TERRA_PROFILE_DEFINE_UPDATE ( f64 )
TERRA_PROFILE_DEFINE_UPDATE ( time )
#endif

// time goes to the bottom because otherwise it fucks up MVS 2017 Intellisense...

#ifdef _WIN32
// TODO make a header that only wraps including windows.h with proper defines and use it everywhere?
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>

LARGE_INTEGER terra_clock_frequency;

void terra_clock_init() {
    QueryPerformanceFrequency ( &terra_clock_frequency );
}

// Returning the performance counter in microseconds rather than seconds so that
// average operations return meaningful values on smaller data and don't sum up to
// something < 1 which will just return 0. Note that this can also be done by changing
// the signature of `terra_clock_to_*` to take a real number rather than an integer.
TerraClockTime terra_clock() {
    LARGE_INTEGER ts;
    QueryPerformanceCounter ( &ts );
    return ( TerraClockTime ) ts.QuadPart * 1000 * 1000;
}

double terra_clock_to_ms ( TerraClockTime delta_time ) {
    return ( ( double ) delta_time / ( terra_clock_frequency.QuadPart * 1000 ) );
}

double terra_clock_to_us ( TerraClockTime delta_time ) {
    return ( ( double ) delta_time / terra_clock_frequency.QuadPart );
}

#else
#include <time.h>

void terra_clock_init() {
}

TerraClockTime terra_clock() {
    return clock();
}

double terra_clock_elapsed_ms ( TerraClockTime delta_time ) {
    return ( double ) ( delta_time / CLOCKS_PER_SEC * 1000 );
}

#endif

#ifdef __cplusplus
}
#endif