#pragma once

// satellite
#include <Messenger.hpp>

// libc++
#include <string>

enum MessageTypes {
    MSG_LOAD_SCENE = 0,
    MSG_SAVE_RENDER_TARGET,

    MSG_CONSOLE_UPDATE_STATE,
    
    MESSAGE_TYPE_COUNT
};

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

enum PanelStates {
    PANEL_STATE_HIDE,
    PANEL_STATE_    
};
using PanelState = int;

struct MessageUpdateConsoleState : public MessagePayload {
    PanelState state;
};