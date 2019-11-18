// Satellite
#include <Messenger.hpp>

// libc++
#include <cassert>

using namespace std;

namespace {
    using MessageListenMask = std::vector<bool>;

    struct Message {
        MessagePayloadPtr data;
        MessageType       type;
    };

    struct Impl {
        // It should be one per time
        std::mutex                     mutex;
        std::vector<MessageListener*>  listeners;
        std::vector<MessageListenMask> listener_masks;
        std::vector<Message>           messages;
    };

    static Impl impl;
}

void Messenger::register_listener(
    MessageListener* listener,
    const std::initializer_list<MessageType>& message_types
) {
    lock_guard<mutex> lock(impl.mutex);
    impl.listeners.emplace_back(listener);

    MessageListenMask mask(MESSAGE_TYPE_COUNT, false);
    for (MessageType type : message_types) {
        mask[type] = true;
    }
}

void Messenger::send (const MessageType type, MessagePayload* data) {
    lock_guard<mutex> lock(impl.mutex);
    Message msg;
    msg.type = type;
    msg.data = std::shared_ptr<MessagePayload>(data);
    impl.messages.emplace_back(move(msg));
}

int Messenger::dispatch() {
    lock_guard<mutex> lock(impl.mutex);

    int dispatched = 0;
    for (Message& msg : impl.messages) {
        for (size_t i = 0; i < impl.listeners.size(); ++i) {
            if (impl.listeners[msg.type]) {
                assert(msg.data);
                impl.listeners[i]->on_message(msg.type, *msg.data);
            }
        }
    }

    impl.messages.clear();
    return dispatched;
}