#pragma once
#include <string>
#include <utility>

using namespace std;

// Identifies who wrote a message in the conversation.
enum class Role { System, User, Assistant };

class Message {
public:
    // Array for messages before they appear.
    Message() : role_(Role::System), content_() {}

    Message(Role role, string content)
        : role_(role), content_(std::move(content)) {}

    Role role() const noexcept { return role_; }
    const string& content() const noexcept { return content_; }

private:
    Role role_;
    string content_;
};
