// satellite
#include <Object.hpp>
#include <LinearAlgebra.hpp>

// terra
#include <Terra.h>

// libc++
#include <cassert>

Object::Object() {
    mat4_identity(world_from_object);
    mat4_identity(world_from_object_invT);
}

RenderData::~RenderData() {
    _delete_vertex_layout();
}

void RenderData::construct_vertex_layout() {
    _delete_vertex_layout();

    Buffer<float> vertex_streams[6] = {
        x, y, z, nx, ny, nz
    };
    
    for (size_t m = 0; m < submeshes.size(); ++m) {
        GLuint vao;
        glGenVertexArrays(1, &vao); GL_NO_ERROR;
        glBindVertexArray(vao); GL_NO_ERROR;

        for (int i = 0; i < 6; ++i) {
            glVertexArrayVertexBuffer(vao, i, vertex_streams[i].buf, 0, vertex_streams[i].stride()); GL_NO_ERROR;
            glEnableVertexArrayAttrib(vao, i); GL_NO_ERROR;
            glVertexArrayAttribFormat(vao, i, vertex_streams[i].stride(), GL_FLOAT, GL_FALSE, 0); GL_NO_ERROR;
            glVertexArrayAttribBinding(vao, i, i); GL_NO_ERROR;
        }

        assert(glIsBuffer(submeshes[m].faces.buf) == GL_TRUE);
        glVertexArrayElementBuffer(vao, submeshes[m].faces.buf); GL_NO_ERROR;
        submeshes[m].vao = vao;
    }
}

void RenderData::bind_vertex_input(const int submesh) const {
    assert(submesh >= 0 && submesh < submeshes.size());
    assert(glIsVertexArray(submeshes[submesh].vao));
    glBindVertexArray(submeshes[submesh].vao);
}

void RenderData::_delete_vertex_layout() {
}