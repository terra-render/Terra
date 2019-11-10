#pragma once

namespace OS {
    void* allocate (size_t bytes);
    void free(void* ptr);

    // free this
    char* read_file_to_string(
        const char* file
    );
}