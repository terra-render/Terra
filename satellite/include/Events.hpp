#pragma once

// stdlib
#include <string>
#include <memory>

struct Event { Event(int type) : type(type) { };  int type; };
using EventPtr = std::shared_ptr<Event>;

enum Events
{
    LoadFile = 0
};

struct LoadFileEvent : public Event
{
    LoadFileEvent() : Event(Events::LoadFile) { }

    std::string str;
};