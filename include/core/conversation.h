#ifndef MINIHARNESS_CORE_CONVERSATION_H
#define MINIHARNESS_CORE_CONVERSATION_H

#include <cstddef>
#include <utility>

#include "c:/Users/Athena/Documents/School/NC_State/2026_Fall/ECE_309/VSCode/MiniHarness/include/core/message.h"

class Conversation {
public:
    Conversation() = default;                       
    ~Conversation();                                

    // Rule of Five
    Conversation(const Conversation& other);         // deep copy
    Conversation& operator=(const Conversation& other);
    Conversation(Conversation&& other) noexcept;   // steal pointer
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message m);

    std::size_t    size() const noexcept { return size_; }
    const Message& at(std::size_t i) const { return data_[i]; }
    const Message* begin() const noexcept { return data_; }
    const Message* end()   const noexcept { return data_ + size_; }

    
    const Message* data() const noexcept { return data_; }

private:
    void grow();                                    // reallocate 

    Message*    data_ = nullptr;
    std::size_t size_     = 0;
    std::size_t capacity_ = 0;
};

#endif 