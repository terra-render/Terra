#pragma once

// libc++
#include <cstdint>
#include <string>
#include <vector>
#include <cassert>
#include <memory>

// satellite
#include <OS.hpp>
#include <Graphics.hpp>
#include <LinearAlgebra.hpp>

// gl3w
#include <GL/gl3w.h>

template <typename T>
struct Buffer {
    ~Buffer();

    void free();
    void allocate(
        const GLenum usage,
        const size_t count, 
        const T* ptr
    );
    void upload();

    std::shared_ptr<T> ptr;
    GLenum usage = GL_INVALID_ENUM;
    size_t count = 0;
    GLuint buf = -1;

    constexpr size_t stride() { return sizeof(T); }
};

struct RenderData {
    ~RenderData();

    using MaterialID = uint64_t;
    
    struct Submesh {
        MaterialID material;
        Buffer<uint32_t> faces;
        GLuint vao;
    };

    void construct_vertex_layout();
    void bind_vertex_input(const int submesh) const;

    Buffer<float> x;
    Buffer<float> y;
    Buffer<float> z;
    Buffer<float> nx;
    Buffer<float> ny;
    Buffer<float> nz;
    std::vector<Submesh> submeshes;

private:
    void _delete_vertex_layout();
};

struct Object {
    Object();
    
    using ID = uint64_t;
    static const ID ID_NULL = (ID)-1;

    ID          id;
    std::string name;
    RenderData  render;
    mat4 world_from_object;
    mat3 world_from_object_invT;
};

///
template <typename T>
Buffer<T>::~Buffer() {
    free();
}

template <typename T>
void Buffer<T>::free() {
    ptr.reset();

//    if (glIsBuffer(buf)) {
//        glDeleteBuffers(1, &buf);
//    }
}

template <typename T>
void Buffer<T>::allocate(
    const GLenum usage,
    const size_t count, 
    const T* data
) {
    assert(count);

    ptr.reset(new T[count]);

    glGenBuffers(1, &buf);
    
    this->count = count;
    this->usage = usage;

    // todo: figure out a nice way to transfer the ownership
    if (data) {
        memcpy(ptr.get(), data, sizeof(T) * count);
    }

    upload();
}

template <typename T>
void Buffer<T>::upload() {
    assert(count);
    assert(usage != GL_INVALID_ENUM);

    glBindBuffer(usage, buf);
    glBufferData(usage, sizeof(T) * count, ptr.get(), GL_STATIC_DRAW);
}