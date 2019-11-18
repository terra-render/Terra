#pragma once

// satellite
#include <Messenger.hpp>

// libc++

struct MessageLoadScene : public MessagePayload {
    std::string path;
};

struct MessageSaveRenderTarget : public MessagePayload {
    std::string path;
};

struct MessageResize : public MessagePayload {
    unsigned int width = -1;
    unsigned int height = -1;
};
