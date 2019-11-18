#pragma once

// libc++
#include <memory>
#include <vector>
#include <thread>
#include <initializer_list>
#include <mutex>

enum MessageType {
    MSG_LOAD_SCENE = 0,
    MSG_SAVE_RENDER_TARGET = 1,
    MSG_SAVE_
    MESSAGE_TYPE_COUNT
};

struct MessagePayload { 
    virtual ~MessagePayload() = default;
};
using MessagePayloadPtr = std::shared_ptr<MessagePayload>;

struct MessageListener {
    virtual ~MessageListener() = default;

    virtual void on_message(
        MessageType type,
        const MessagePayload& data
    ) = 0;
};

namespace Messenger {
    void register_listener(
        MessageListener* listener,
        const std::initializer_list<MessageType>& messages
    );

    void send(
        const MessageType type, 
        MessagePayload* data
    );

    int dispatch();  
}

// please map 1-1 msg_type and msg and make this two arguments
#define SEND_MESSAGE(type, msg_type, msg) Messenger::send(type, new msg_type(std::move(msg)))