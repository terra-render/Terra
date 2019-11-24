// satellite
#include <Panels/TextureVisualizer.hpp>
#include <Messages.hpp>
#include <OS.hpp>

TextureVisualizer::TextureVisualizer(const PanelSharedPtr& parent) : 
Panel::Panel(parent, "TextureVisualizer") {
}

void TextureVisualizer::init() {
    Messenger::register_listener(
        [this](const MessageType type, const MessagePayload& _data) {
            switch (type) {
            case MSG_SET_RENDER_VIEW: {
                _texture = ((MessageSetRenderView&)_data).image;
                break;
            }
            case MSG_DETACH_RENDER_VIEW: {
                _texture.reset();
                break;
            }
            }
        }, {
            MSG_SET_RENDER_VIEW,
            MSG_DETACH_RENDER_VIEW
        });

    char* vert_src = OS::read_file_to_string("shaders/quad.vert.glsl");
    char* frag_src = OS::read_file_to_string("shaders/quad.frag.glsl");

    _pipeline.reset(
        nullptr, nullptr,
        vert_src, 
        frag_src,
        false
    );

    OS::free(vert_src);
    OS::free(frag_src);
}

void TextureVisualizer::draw() {
    _pipeline.bind();
    glClearColor(0.15f, 0.15f, 0.15f, 1.f);
    glClearDepth(1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (_texture && _texture->valid()) {
        const ShaderUniform& u_texture = _pipeline.uniform("u_texture");
        glActiveTexture(GL_TEXTURE0 + u_texture.binding); GL_NO_ERROR;
        glBindTexture(GL_TEXTURE_2D, _texture->id); GL_NO_ERROR;
    }
    glDrawArrays(GL_TRIANGLES, 0, 3); GL_NO_ERROR;
}