// Header
#include <Logging.hpp>

// Satellite
#include <Console.hpp>

// stdlib
#include <cstdarg>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

namespace {
    FILE*    info_fp    = nullptr;
    FILE*    warning_fp = nullptr;
    FILE*    error_fp   = nullptr;
    FILE*    verbose_fp = nullptr;
    Console* console_p  = nullptr;

    vector<string> console_buf;

    void vbuf ( const char* fmt, va_list args ) {
        va_list args_cp;
        va_copy ( args_cp, args );
        size_t len = vsnprintf ( nullptr, 0, fmt, args );
        string s;
        s.resize ( len );
        vsnprintf ( &s[0], len + 1, fmt, args_cp );
        console_buf.push_back ( s );
    }

    void buf ( const char* /*fun*/, const char* fmt, ... ) {
        va_list args;
        va_start ( args, fmt );
        vbuf ( fmt, args );
        va_end ( args );
    }

    void print ( FILE* fp, const char* fun, const char* tag, const char* fmt, va_list args ) {
        if ( ( fp != stdout && fp != stderr ) || console_p == nullptr ) {
            string new_fmt = tag;
            new_fmt += "> ";
            new_fmt += fmt;
            new_fmt += " @ ";
            new_fmt += fun;
            new_fmt += '\n';

            vfprintf ( fp, new_fmt.c_str(), args );
        }

        if ( console_p ) {
            string new_fmt = tag;
            new_fmt += "> ";
            new_fmt += fmt;
            new_fmt += "\n";

            console_p->vprintf ( new_fmt.c_str(), args );
        } else {
            vbuf ( fmt, args );
        }
    }

}

namespace Log {
    void set_targets ( FILE* info_fp, FILE* warning_fp, FILE* error_fp, FILE* verbose_fp ) {
        ::info_fp    = info_fp;
        ::warning_fp = warning_fp;
        ::error_fp   = error_fp;
        ::verbose_fp = verbose_fp;
    }

    void redirect_console ( Console* console ) {
        console_p = console;
    }

#define GEN_LOG(name, tag)\
    void name (const char* fun, const char* fmt, ...) {\
        va_list args; va_start(args, fmt); \
        print( name##_fp, fun, tag, fmt, args);\
        va_end(args); \
    }

    GEN_LOG ( verbose, "verb" );
    GEN_LOG ( info, "info" );
    GEN_LOG ( warning, "warn" );
    GEN_LOG ( error, "err " );

    void console ( const char* fmt, ... ) {
        if ( console_p == nullptr ) {
            return;
        }

        va_list args;
        va_start ( args, fmt );
        console_p->printf ( fmt, args );
        va_end ( args );
    }

    void flush() {
        if ( console_p ) {
            for ( const string& s : console_buf ) {
                console_p->printf ( "boot> %s", s.c_str() );
            }
        }
    }
}