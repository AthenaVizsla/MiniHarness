#ifndef MINIHARNESS_CORE_MESSAGE_H
#define MINIHARNESS_CORE_MESSAGE_H

#include <string>

enum class Role { System, User, Assistant };

class Message {
public:

    Message() : role_(Role::User), content_() {}

    Message(Role role, std::string content)
        : role_(role), content_(std::move(content)) {}

    Role               role()    const noexcept { return role_; }
    const std::string& content() const noexcept { return content_; }

private:
    Role        role_;
    std::string content_;
};

#endif 