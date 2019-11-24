#pragma once

// libc++
#include <string>

namespace OS {
    void* allocate (
        const size_t bytes
    );


    void free(
        void* ptr
    );

    // OS::free this
    char* read_file_to_string(
        const char* file
    );

    bool file_exists(
        const char* filename
    );

    std::string path_join(
        const char* lhs,
        const char* rhs
    );
}
