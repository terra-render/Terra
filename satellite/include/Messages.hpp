#pragma once

// satellite
#include <Messenger.hpp>

// libc++
#include <string>

enum MessageTypes {
    MSG_LOAD_SCENE = 0,
    MSG_SAVE_RENDER_TARGET,
    
    MSG_SET_RENDERER,
    MSG_SET_CAMERA,
    MSG_SET_CAMERA_CONTROLS,

    MSG_SET_RENDER_VIEW,

    MSG_CONSOLE_UPDATE_STATE,
    
    MESSAGE_TYPE_COUNT
};

struct MessageLoadScene : public MessagePayload {
    std::string path;
};

struct MessageSaveRenderTarget : public MessagePayload {
    std::string path;
};

struct MessageClearRenderTarget : public MessagePayload { };

struct MessageSetRenderView : public MessagePayload {
    // texture handle
};

struct MessageSetRenderer : public MessagePayload {
    std::string type;
};

struct MessageSetCamera : public MessagePayload {
    std::string type;
};

struct MessageSetCameraControls : public MessagePayload {
    std::string type;
};

struct MessageResize : public MessagePayload {
    unsigned int width = -1;
    unsigned int height = -1;
};

enum PanelStates {
    PANEL_STATE_HIDE,
    PANEL_STATE_SHOW
};
using PanelState = int;

struct MessageUpdateConsoleState : public MessagePayload {
    PanelState state;
};