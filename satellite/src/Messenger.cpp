// Satellite
#include <Messenger.hpp>
#include <Messages.hpp>

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
        mutex mutex;
        vector<MessageListener>   listeners;
        vector<MessageListenMask> listener_masks;
        vector<Message>           messages;
    };

    static Impl impl;
}

void Messenger::register_listener(
    const MessageListener listener,
    const std::initializer_list<MessageType>& message_types
) {
    lock_guard<mutex> lock(impl.mutex);
    impl.listeners.emplace_back(listener);

    MessageListenMask mask(MESSAGE_TYPE_COUNT, false);
    for (MessageType type : message_types) {
        mask[type] = true;
    }
    impl.listener_masks.emplace_back(move(mask));
}

void Messenger::send (const MessageType type, MessagePayload* data) {
    lock_guard<mutex> lock(impl.mutex);
    Message msg;
    msg.type = type;
    msg.data = std::shared_ptr<MessagePayload>(data);
    impl.messages.emplace_back(move(msg));
}

void Messenger::dispatch() {
    size_t next_msg = 0;
    impl.mutex.lock();
    while (next_msg < impl.messages.size()) {
        auto msg = impl.messages[next_msg];
        impl.mutex.unlock(); // We want to be able to listeners to send new messages

        for (size_t i = 0; i < impl.listeners.size(); ++i) {
            if (impl.listener_masks[i][msg.type]) {
                assert(msg.data);
                impl.listeners[i](msg.type, *msg.data);
            }
        }

        ++next_msg;
        impl.mutex.lock();
    }
    impl.messages.clear();
    impl.mutex.unlock();
}