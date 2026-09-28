#pragma once

#include "core/message.h"
#include <cstddef>

using namespace std;

class Conversation {
public:
    Conversation();
    ~Conversation();

    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message message);

    size_t size() const noexcept;
    const Message& at(size_t i) const;

    const Message* begin() const noexcept;
    const Message* end() const noexcept;

private:
    Message* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
};
