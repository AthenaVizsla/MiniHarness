// tests/p2/test_p2.cpp
//
// YOUR test suite goes here. At least 12 assert-based test cases — see
// spec §5 for the required categories and the sample test for the
// expected level of rigor.
//
// This file is a stub so the project builds out of the box; replace the
// body of main() with your own tests.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>

static int tests_run = 0;
#define CHECK(cond) do { ++tests_run; assert(cond); \
    std::printf("ok %2d - %s\n", tests_run, #cond); } while (0)

static void test_append_and_size() {
    Conversation c;
    CHECK(c.size() == 0);          // empty conversation, no OOB access
    c.append(Message(Role::User, "hello"));
    c.append(Message(Role::Assistant, "Hi!"));
    CHECK(c.size() == 2);
    CHECK(c.at(1).content() == "Hi!");
}

static void test_copy_is_deep() {
    Conversation a;
    a.append(Message(Role::User, "x"));
    Conversation b = a;            // copy ctor
    CHECK(b.data() != a.data());   // different buffer addresses
    b.append(Message(Role::User, "y"));
    CHECK(a.size() == 1);          // original unaffected
    CHECK(b.size() == 2);
}

static void test_move_steals() {
    Conversation a;
    a.append(Message(Role::User, "x"));
    const Message* original = a.data();
    Conversation b = std::move(a);
    CHECK(b.data() == original);   // pointer stolen
    CHECK(a.data() == nullptr);   // source zeroed
    CHECK(a.size() == 0);
}

static void test_scanner_clean() {
    SentinelScanner s("<|end_conversation|>");
    auto out = s.feed("Hello there!");
    CHECK(!out.sentinel_found);
    CHECK(out.safe_text == "Hello there!");
}

static void test_scanner_split_every_boundary() {
    const std::string reply = "Goodbye.<|end_conversation|>";
    for (std::size_t split = 1; split < reply.size() - 1; ++split) {
        SentinelScanner s("<|end_conversation|>");
        auto o1 = s.feed(reply.substr(0, split));
        auto o2 = s.feed(reply.substr(split));
        CHECK(o2.sentinel_found);
        CHECK(o1.safe_text + o2.safe_text == "Goodbye.");
        // pending_ must never exceed sentinel length - check via flush: nothing left
    }
}

static void test_scanner_false_alarm() {
    SentinelScanner s("<|end_conversation|>");
    auto out = s.feed("bye <|end_world|> ok");
    CHECK(!out.sentinel_found);
    CHECK(out.safe_text == "bye <|end_world|> ok");
}

int main() {
    // TODO: write your tests here.
        test_append_and_size();
    test_copy_is_deep();
    test_move_steals();
    test_scanner_clean();
    test_scanner_split_every_boundary();
    test_scanner_false_alarm();
    std::printf("All %d checks passed.\n", tests_run);
    return 0;
}
