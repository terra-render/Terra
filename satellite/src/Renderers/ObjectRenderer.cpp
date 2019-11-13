// satellite
#include <Renderers/ObjectRenderer.hpp>
#include <Object.hpp>
#include <Scene.hpp>
#include <Graphics.hpp>
#include <OS.hpp>

ObjectRenderer::ObjectRenderer() : 
Renderer::Renderer() {

    char* shader_vert_text = OS::read_file_to_string("shaders/object.vert.glsl");
    char* shader_frag_text = OS::read_file_to_string("shaders/object.frag.glsl");

    _pipeline.load (
        shader_vert_text,
        shader_frag_text,
        100, 100
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

    for (const Object& o : scene.objects()) {
        o.render.bind_vertex_input();
        
        for (const RenderData::Submesh& mesh : o.render.submeshes) {
            glDrawElements(GL_TRIANGLES, mesh.faces.count / 3, GL_UNSIGNED_INT, nullptr); GL_NO_ERROR; 
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
