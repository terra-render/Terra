#pragma once

// stdlib
#include <cstdio>
#include <string>

class Console;

namespace Log  {
    // Set targets for all log channels
    // nullptr to disable
    void set_targets ( FILE* info_fp = stdout, FILE* warning_fp = stderr, FILE* error_fp = stderr, FILE* verbose_fp = nullptr );

    // Redirects all the outputs (except verbose) to the console too
    // nullptr to disable
    void redirect_console ( Console* _console );

    // Can be also redirected to the internal console
    void verbose ( const char* fun, const char* fmt, ... );
    void info ( const char* fun, const char* fmt, ... );
    void warning ( const char* fun, const char* fmt, ... );
    void error ( const char* fun, const char* fmt, ... );

    // Internal console only
    void console ( const char* fmt, ... );
    void buf ( const char* fun, const char* fmt, ... );
    void flush();
}

#define FMT(fmt, ...) __FUNCTION__, fmt, __VA_ARGS__
#define STR(str)      __FUNCTION__, str