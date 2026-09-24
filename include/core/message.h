#pragma once

#include <string>

// Who wrote this message?
enum class Role { System, User, Assistant };

class Message {
public:
    // An empty message is needed when Conversation makes a new array.
    Message() : role_(Role::System), content_() {}

    Message(Role role, std::string content)
        : role_(role), content_(content) {}

    Role role() const noexcept { return role_; }
    const std::string& content() const noexcept { return content_; }

private:
    Role role_;
    std::string content_;
};
