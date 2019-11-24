// satellite
#include <OS.hpp>

// libc
#include <cstdio>
#include <cassert>
#include <fstream>

// win32
#include <Windows.h>

namespace OS {
    void* allocate(
        const size_t bytes
    ) {
        return malloc(bytes);
        //return _aligned_malloc(bytes, 16);
    }

    void free(
        void* ptr
    ) {
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

    bool file_exists(const char* filename) {
        return std::ifstream(filename).good();
    }

    std::string path_join(
        const char* lhs,
        const char* rhs
    ) {
        assert(lhs);
        assert(rhs);
        std::string path = lhs;
        if (path.back() != '/' && path.back() != '\\') {
            path.append("/");
        }

        const int offset = (rhs[0] == '\\' || rhs[0] == '/') ? 1 : 0;
        path.append(rhs + offset);
        return path;
    }
}