#pragma once

// libc++
#include <memory>
#include <vector>
#include <thread>
#include <initializer_list>
#include <functional>
#include <mutex>

using MessageType = unsigned int;

struct MessagePayload { 
    virtual ~MessagePayload() = default;
};
using MessagePayloadPtr = std::shared_ptr<MessagePayload>;

using MessageListener = std::function<void(
    const MessageType type,
    const MessagePayload& data
)>;

namespace Messenger {
    void register_listener(
        const MessageListener listener,
        const std::initializer_list<MessageType>& messages
    );

    void send(
        const MessageType type, 
        MessagePayload* data
    );

    void dispatch();  
}

// please map 1-1 msg_type and msg and make this two arguments
#define SEND_MESSAGE(type, msg_type, msg) Messenger::send(type, new msg_type(msg))