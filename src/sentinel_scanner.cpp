
#include "core/sentinel_scanner.h"

#include <algorithm>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

std::size_t SentinelScanner::match_length(std::string_view pending, char c) const {

    const std::size_t max_len = std::min(pending.size() + 1, sentinel_.size());

    // Try candidate lengths from longest to shortest so we find the
    // *longest* matching suffix, not just any matching suffix.
    for (std::size_t len = max_len; len > 0; --len) {
        // A candidate suffix of length `len` consists of the last
        // (len - 1) characters of `pending`, followed by `c`.
        const std::size_t pending_start = pending.size() - (len - 1);

        bool matches = true;
        for (std::size_t i = 0; i < len - 1; ++i) {
            if (pending[pending_start + i] != sentinel_[i]) {
                matches = false;
                break;
            }
        }
        if (matches && sentinel_[len - 1] == c) {
            return len;
        }
    }
    return 0;
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    Out out;
    out.sentinel_found = false;
    out.safe_text.reserve(chunk.size());

    for (char c : chunk) {
        const std::size_t len = match_length(pending_, c);

        if (len == sentinel_.size() && sentinel_.size() > 0) {
            out.sentinel_found = true;
            pending_.clear();
            return out;
        }

        const std::size_t old_size = pending_.size();
        const std::size_t total = old_size + 1;
        const std::size_t flush_count = total - len;

        pending_.push_back(c);                             
        out.safe_text.append(pending_, 0, flush_count);      
        pending_.erase(0, flush_count);                      
    }

    return out;
}

SentinelScanner::Out SentinelScanner::flush() {
    Out out{std::move(pending_), false};
    pending_.clear();
    return out;
}