// satellite
#include <OS.hpp>

// libc
#include <cstdio>
#include <cassert>

// win32
#include <Windows.h>

namespace OS {
    void* allocate(size_t bytes) {
        return malloc(bytes);
        //return _aligned_malloc(bytes, 16);
    }

    void free(void* ptr) {
        ::free(ptr);
        //_aligned_free(ptr);
    }

    char* read_file_to_string(
        const char* file
    ) {
        FILE* fp = fopen(file, "rb");
        if (!fp) {
            return NULL;
        }

        fseek(fp, 0, SEEK_END);
        const size_t file_len = (size_t)ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char* buf = nullptr;
        if (file_len == 0) {
            goto exit;
        }

        buf = (char*)OS::allocate(file_len + 1);
        assert(fread(buf, 1, file_len, fp) == file_len);
        buf[file_len] = '\0';

exit:
        fclose(fp);
        return buf;
    }
}