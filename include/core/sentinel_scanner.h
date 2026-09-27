#ifndef MINIHARNESS_CORE_SENTINEL_SCANNER_H
#define MINIHARNESS_CORE_SENTINEL_SCANNER_H

#include <string>
#include <string_view>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel);

    struct Out { std::string safe_text; bool sentinel_found; };

    Out feed(std::string_view chunk);
    Out flush();   // call at end of stream: emit whatever is pending

private:
    // Longest suffix of pending_+c that is a prefix of sentinel_, or 0.
    std::size_t match_length(std::string_view pending, char c) const;

    std::string sentinel_;
    std::string pending_;   // invariant: pending_.size() < sentinel_.size()
};

#endif // MINIHARNESS_CORE_SENTINEL_SCANNER_H