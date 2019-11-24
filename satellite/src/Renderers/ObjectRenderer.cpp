// satellite
#include <Renderers/ObjectRenderer.hpp>
#include <Object.hpp>
#include <Scene.hpp>
#include <Graphics.hpp>
#include <OS.hpp>
#include <LinearAlgebra.hpp>
#include <Camera.hpp>

ObjectRenderer::ObjectRenderer() : 
Renderer::Renderer() {
    char* shader_vert_text = OS::read_file_to_string("shaders/object.vert.glsl"); 
    char* shader_frag_text = OS::read_file_to_string("shaders/object.frag.glsl");

    assert(_render_target->valid());
    _rt_depth = Image::create(_render_target->width, _render_target->height, GL_DEPTH_COMPONENT32F);

    _pipeline.reset (
        _render_target,
        _rt_depth,
        shader_vert_text,
        shader_frag_text,
        true
    );

    OS::free(shader_vert_text);
    OS::free(shader_frag_text);
}

ObjectRenderer::~ObjectRenderer() {

}

void ObjectRenderer::clear() {
    
}

void ObjectRenderer::update(
    const Scene& scene,
    const Camera& camera
) {
    _pipeline.bind();

    const ShaderUniform& u_clip_from_world = _pipeline.uniform("u_clip_from_world");
    assert(u_clip_from_world.type == GL_FLOAT_MAT4);
    const ShaderUniform& u_world_from_object = _pipeline.uniform("u_world_from_object");
    assert(u_world_from_object.type == GL_FLOAT_MAT4);
    const ShaderUniform& u_world_from_object_invT = _pipeline.uniform("u_world_from_object_invT");
    assert(u_world_from_object_invT.type == GL_FLOAT_MAT3);

    glClearColor(.15f, .15f, .15f, 1.f); GL_NO_ERROR;
    glClearDepth(1.); GL_NO_ERROR;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); GL_NO_ERROR;

    mat4 clip_from_world;
    mat4_mul(clip_from_world, *camera.clip_from_view(), *camera.view_from_world());
    glUniformMatrix4fv(u_clip_from_world.binding, 1, GL_FALSE, (const GLfloat*)clip_from_world); GL_NO_ERROR;

    for (const Object& o : scene.objects()) {
        if (o.render.flags & RENDER_FLAG_HIDE) {
            continue;
        }
    
        glUniformMatrix4fv(u_world_from_object.binding, 1, GL_FALSE, (const GLfloat*)o.world_from_object); GL_NO_ERROR;
        glUniformMatrix3fv(u_world_from_object_invT.binding, 1, GL_FALSE, (const GLfloat*)o.world_from_object_invT); GL_NO_ERROR;

        if (o.render.flags & RENDER_FLAG_DOUBLE_SIDED) {
            glDisable(GL_CULL_FACE);
        } else {
            glFrontFace(GL_CCW);
            glEnable(GL_CULL_FACE);
        }

        for (size_t i = 0; i < o.render.submeshes.size(); ++i) {
            o.render.bind_vertex_input(i);
            glDrawElements(GL_TRIANGLES, o.render.submeshes[i].faces.count, GL_UNSIGNED_INT, nullptr); GL_NO_ERROR;
        }
    }
}

void ObjectRenderer::start() {
    Renderer::start();
}

void ObjectRenderer::pause() {
    Renderer::pause();
}

void ObjectRenderer::update_settings(const Settings& settings) {
    Renderer::update_settings(settings);
}
