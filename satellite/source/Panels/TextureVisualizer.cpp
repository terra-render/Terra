// satellite
#include <Panels/TextureVisualizer.hpp>
#include <Messages.hpp>

void TextureVisualizer::init() {
    Messenger::register_listener(
        [this](const MessageType type, const MessagePayload& data) {

        }, {
            MSG_SET_RENDER_VIEW
        });
}

void TextureVisualizer::draw() {

}