#include "core/conversation.h"

Conversation::~Conversation() { delete[] data_; }

// Copy: deep copy — new buffer, copied element-by-element.
Conversation::Conversation(const Conversation& other)
    : data_(other.capacity_ ? new Message[other.capacity_] : nullptr),
      size_(other.size_), capacity_(other.capacity_) {
    for (std::size_t i = 0; i < size_; ++i) data_[i] = other.data_[i];
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this != &other) {
        Message* fresh = other.capacity_ ? new Message[other.capacity_] : nullptr;
        for (std::size_t i = 0; i < other.size_; ++i) fresh[i] = other.data_[i];
        delete[] data_;                 // free old buffer AFTER allocating
        data_ = fresh;                  // (gives the strong exception guarantee)
        size_ = other.size_;
        capacity_ = other.capacity_;
    }
    return *this;
}

// Move: steal the pointer, zero the source so its dtor is a no-op.
Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;  size_ = other.size_;  capacity_ = other.capacity_;
        other.data_ = nullptr; other.size_ = 0;     other.capacity_ = 0;
    }
    return *this;
}

void Conversation::grow() {
    std::size_t new_cap = capacity_ ? capacity_ * 2 : 4;
    Message* fresh = new Message[new_cap];
    for (std::size_t i = 0; i < size_; ++i) fresh[i] = data_[i];
    delete[] data_;
    data_ = fresh;
    capacity_ = new_cap;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) grow();
    data_[size_++] = std::move(m);
}