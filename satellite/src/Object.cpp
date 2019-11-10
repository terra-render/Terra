// satellite
#include <Object.hpp>

// terra
#include <Terra.h>

// libc++
#include <cassert>

RenderData::~RenderData() {
    _delete_vertex_layout();
}

void RenderData::construct_vertex_layout() {
    _delete_vertex_layout();

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    Buffer<float> vertex_streams[6] = {
        x, y, z, nx, ny, nz
    };

    for (int i = 0; i < 6; ++i) {
        glVertexArrayVertexBuffer(vao, i, vertex_streams[i].buf, 0, vertex_streams[i].stride()); GL_NO_ERROR;
        glEnableVertexArrayAttrib(vao, i); GL_NO_ERROR;
        glVertexArrayAttribFormat(vao, i, vertex_streams[i].stride(), GL_FLOAT, GL_FALSE, 0); GL_NO_ERROR;
        glVertexArrayAttribBinding(vao, i, i); GL_NO_ERROR;
    }
    
}

void RenderData::bind_vertex_input() const {
    assert(glIsVertexArray(vao));
    glBindVertexArray(vao); GL_NO_ERROR;
}

void RenderData::_delete_vertex_layout() {
    if (glIsVertexArray(vao)) {
        glDeleteVertexArrays(1, &vao); GL_NO_ERROR;
    }
}